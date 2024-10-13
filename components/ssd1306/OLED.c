#include "OLED.h"
#include <stdlib.h>
#include <limits.h>

static const char *TAG = "OLED";
#define ESP_INTR_FLAG_DEFAULT 0

// Funções privadas
static void initializeReset(OLED* self, int reset);
static esp_err_t initializeI2C(OLED* self, int sda, int scl);
static void initialize(OLED* self);
static int writeRegister(OLED* self, uint8_t addr, uint8_t reg, uint8_t value);
static int writeData(OLED* self, uint8_t addr, uint8_t* buf, int len);
static void sendCommand(OLED* self, uint8_t command);
static void drawStringInternal(OLED* self, int16_t xMove, int16_t yMove, char* text, uint16_t textLength, uint16_t textWidth, OLEDDISPLAY_COLOR color);
static void drawInternal(OLED* self, int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *data, uint16_t offset, uint16_t bytesInData, OLEDDISPLAY_COLOR color);

OLED* OLED_create(int width, int height, int sda, int scl, int reset) {
    OLED* self = (OLED*) malloc(sizeof(OLED));
    if (!self) return NULL;

    self->_displayWidth = width;
    self->_displayHeight = height;
    self->_displayBufferSize = self->_displayWidth * self->_displayHeight / 8;

    self->_buffer = (uint8_t*) malloc(sizeof(uint8_t) * self->_displayBufferSize);
    self->_buffer_back = (uint8_t*) malloc(sizeof(uint8_t) * self->_displayBufferSize);
    memset(self->_buffer, 0, self->_displayBufferSize);
    memset(self->_buffer_back, 0, self->_displayBufferSize);

    self->fontData = ArialMT_Plain_16; // Certifique-se de que esta fonte está definida

    // Armazenar pinos na estrutura
    self->sda_pin = sda;
    self->scl_pin = scl;
    self->reset_pin = reset;

    // Inicializar I2C e reset
    initializeReset(self, reset);
    initializeI2C(self, sda, scl);
    initialize(self);

    return self;
}

void OLED_destroy(OLED* self) {
    if (self) {
        if (self->_buffer) {
            free(self->_buffer);
            self->_buffer = NULL;
        }
        if (self->_buffer_back) {
            free(self->_buffer_back);
            self->_buffer_back = NULL;
        }

        // Desinstalar o driver I2C
        i2c_driver_delete(self->i2c_master_port);

        // Resetar pinos GPIO
        gpio_reset_pin(self->sda_pin);
        gpio_reset_pin(self->scl_pin);
        gpio_reset_pin(self->reset_pin);

        // Opcional: Limpar campos da estrutura
        self->i2c_master_port = -1;
        self->sda_pin = -1;
        self->scl_pin = -1;
        self->reset_pin = -1;

        free(self);
    }
}


static void initializeReset(OLED* self, int reset) {
    self->reset_pin = reset; // Armazenar o pino de reset

    gpio_num_t r = (gpio_num_t) reset;

    gpio_pad_select_gpio(r);
    gpio_set_direction(r, GPIO_MODE_OUTPUT);

    gpio_set_level(r, 0);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    gpio_set_level(r, 1);
    vTaskDelay(50 / portTICK_PERIOD_MS);
}

static esp_err_t initializeI2C(OLED* self, int sda, int scl) {
    int i2c_master_port = I2C_MASTER_NUM;
    self->i2c_master_port = i2c_master_port; // Armazenar a porta I2C
    self->sda_pin = sda;                     // Armazenar o pino SDA
    self->scl_pin = scl;                     // Armazenar o pino SCL

    i2c_config_t conf = {0};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = sda;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = scl;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 100000;
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

static void initialize(OLED* self) {
    sendCommand(self, DISPLAYOFF);
    sendCommand(self, SETDISPLAYCLOCKDIV);
    sendCommand(self, 0xF0); // Increase speed of the display max ~96Hz
    sendCommand(self, SETMULTIPLEX);
    sendCommand(self, self->_displayHeight - 1);
    sendCommand(self, SETDISPLAYOFFSET);
    sendCommand(self, 0x00);
    sendCommand(self, SETSTARTLINE);
    sendCommand(self, CHARGEPUMP);
    sendCommand(self, 0x14);
    sendCommand(self, MEMORYMODE);
    sendCommand(self, 0x00);
    sendCommand(self, SEGREMAP | 0x01);
    sendCommand(self, COMSCANDEC);
    sendCommand(self, SETCOMPINS);

    if (self->_displayHeight == 64)
        sendCommand(self, 0x12);
    else
        sendCommand(self, 0x02);

    sendCommand(self, SETCONTRAST);
    sendCommand(self, 0xCF);

    sendCommand(self, SETPRECHARGE);
    sendCommand(self, 0xF1);
    sendCommand(self, SETVCOMDETECT); //0xDB, (additionally needed to lower the contrast)
    sendCommand(self, 0x40);          //0x40 default, to lower the contrast, put 0
    sendCommand(self, DISPLAYALLON_RESUME);
    sendCommand(self, NORMALDISPLAY);
    sendCommand(self, 0x2e);            // stop scroll
    sendCommand(self, DISPLAYON);
}

static int writeData(OLED* self, uint8_t addr, uint8_t* buf, int len) {
    uint8_t buffer = 0x40;
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, buffer, ACK_CHECK_EN);
    i2c_master_write(cmd, buf, len - 1, ACK_VAL);
    i2c_master_write_byte(cmd, buf[len - 1], NACK_VAL);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    return 0;
}

static int writeRegister(OLED* self, uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = value;
    int ret;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, addr << 1 | I2C_MASTER_WRITE, ACK_CHECK_EN);
    i2c_master_write(cmd, buffer, 2, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    if (ret != ESP_OK) {
        return ret;
    }
    return 0;
}

static void sendCommand(OLED* self, uint8_t command) {
    writeRegister(self, OLED_ADDRESS, 0x00, command);
}

void OLED_sendDataBack(OLED* self) {
    uint8_t minBoundY = UINT8_MAX;
    uint8_t maxBoundY = 0;
    uint8_t minBoundX = UINT8_MAX;
    uint8_t maxBoundX = 0;
    uint8_t x, y;

    for (y = 0; y < self->_displayHeight / 8; y++) {
        for (x = 0; x < self->_displayWidth; x++) {
            uint16_t pos = x + y * self->_displayWidth;
            if (self->_buffer[pos] != self->_buffer_back[pos]) {
                minBoundY = fmin(minBoundY, y);
                maxBoundY = fmax(maxBoundY, y);
                minBoundX = fmin(minBoundX, x);
                maxBoundX = fmax(maxBoundX, x);
            }
            self->_buffer_back[pos] = self->_buffer[pos];
        }
    }

    if (minBoundY == UINT8_MAX) return;

    sendCommand(self, COLUMNADDR);
    sendCommand(self, minBoundX);
    sendCommand(self, maxBoundX);

    sendCommand(self, PAGEADDR);
    sendCommand(self, minBoundY);
    sendCommand(self, maxBoundY);

    uint8_t sbuf[16];
    int idx = 0;

    for (y = minBoundY; y <= maxBoundY; y++) {
        for (x = minBoundX; x <= maxBoundX; x++) {
            sbuf[idx++] = self->_buffer[x + y * self->_displayWidth];
            if (idx == 16) {
                writeData(self, OLED_ADDRESS, sbuf, 16);
                idx = 0;
            }
        }
    }

    if (idx > 0)
        writeData(self, OLED_ADDRESS, sbuf, idx);
}

void OLED_sendData(OLED* self) {
    sendCommand(self, COLUMNADDR);
    sendCommand(self, 0);
    sendCommand(self, (self->_displayWidth - 1));
    sendCommand(self, PAGEADDR);
    sendCommand(self, 0x0);

    if (self->_displayHeight == 64)
        sendCommand(self, 0x7);
    else
        sendCommand(self, 0x3);

    for (uint16_t i = 0; i < self->_displayBufferSize / 16; i++) {
        writeData(self, OLED_ADDRESS, &self->_buffer[i * 16], 16);
    }
}

void OLED_clear(OLED* self) {
    memset(self->_buffer, 0, self->_displayBufferSize);
}

void OLED_setPixelColor(OLED* self, int16_t x, int16_t y, OLEDDISPLAY_COLOR color) {
    switch (color) {
        case WHITE:
            self->_buffer[x + (y >> 3) * self->_displayWidth] |= (1 << (y & 7));
            break;
        case BLACK:
            self->_buffer[x + (y >> 3) * self->_displayWidth] &= ~(1 << (y & 7));
            break;
        case INVERSE:
            self->_buffer[x + (y >> 3) * self->_displayWidth] ^= (1 << (y & 7));
            break;
    }
}

static void drawInternal(OLED* self, int16_t xMove, int16_t yMove, int16_t width, int16_t height, const uint8_t *data, uint16_t offset, uint16_t bytesInData, OLEDDISPLAY_COLOR color) {
    if (width < 0 || height < 0) return;
    if (yMove + height < 0 || yMove > self->_displayHeight)  return;
    if (xMove + width  < 0 || xMove > self->_displayWidth )   return;

    uint8_t  rasterHeight = 1 + ((height - 1) >> 3); // fast ceil(height / 8.0)
    int8_t   yOffset      = yMove & 7;

    bytesInData = bytesInData == 0 ? width * rasterHeight : bytesInData;

    int16_t initYMove   = yMove;
    int8_t  initYOffset = yOffset;

    for (uint16_t i = 0; i < bytesInData; i++) {
        if (i % rasterHeight == 0) {
            yMove   = initYMove;
            yOffset = initYOffset;
        }

        uint8_t currentByte = *(data + offset + i);

        int16_t xPos = xMove + (i / rasterHeight);
        int16_t yPos = ((yMove >> 3) + (i % rasterHeight)) * self->_displayWidth;

        int16_t dataPos    = xPos  + yPos;

        if (dataPos >=  0  && dataPos < self->_displayBufferSize && xPos >= 0 && xPos < self->_displayWidth) {
            if (yOffset >= 0) {
                switch (color) {
                    case WHITE:
                        self->_buffer[dataPos] |= currentByte << yOffset;
                        break;
                    case BLACK:
                        self->_buffer[dataPos] &= ~(currentByte << yOffset);
                        break;
                    case INVERSE:
                        self->_buffer[dataPos] ^= currentByte << yOffset;
                        break;
                }

                if (dataPos < (self->_displayBufferSize - self->_displayWidth)) {
                    switch (color) {
                        case WHITE:
                            self->_buffer[dataPos + self->_displayWidth] |= currentByte >> (8 - yOffset);
                            break;
                        case BLACK:
                            self->_buffer[dataPos + self->_displayWidth] &= ~(currentByte >> (8 - yOffset));
                            break;
                        case INVERSE:
                            self->_buffer[dataPos + self->_displayWidth] ^= currentByte >> (8 - yOffset);
                            break;
                    }
                }
            } else {
                yOffset = -yOffset;
                switch (color) {
                    case WHITE:
                        self->_buffer[dataPos] |= currentByte >> yOffset;
                        break;
                    case BLACK:
                        self->_buffer[dataPos] &= ~(currentByte >> yOffset);
                        break;
                    case INVERSE:
                        self->_buffer[dataPos] ^= currentByte >> yOffset;
                        break;
                }
                yMove -= 8;
                yOffset = 8 - yOffset;
            }
        }
    }
}

static void drawStringInternal(OLED* self, int16_t xMove, int16_t yMove, char* text, uint16_t textLength, uint16_t textWidth, OLEDDISPLAY_COLOR color) {
    uint8_t textHeight       = *(self->fontData + HEIGHT_POS);
    uint8_t firstChar        = *(self->fontData + FIRST_CHAR_POS);
    uint16_t sizeOfJumpTable = *(self->fontData + CHAR_NUM_POS)  * JUMPTABLE_BYTES;

    uint16_t cursorX         = 0;
    uint16_t cursorY         = 0;

    if (xMove + textWidth  < 0 || xMove > self->_displayWidth ) {return;}
    if (yMove + textHeight < 0 || yMove > self->_displayHeight ) {return;}

    for (uint16_t j = 0; j < textLength; j++) {
        int16_t xPos = xMove + cursorX;
        int16_t yPos = yMove + cursorY;

        uint8_t code = text[j];
        if (code >= firstChar) {
            uint8_t charCode = code - firstChar;

            uint8_t msbJumpToChar    = *(self->fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES);
            uint8_t lsbJumpToChar    = *(self->fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_LSB);
            uint8_t charByteSize     = *(self->fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_SIZE);
            uint8_t currentCharWidth = *(self->fontData + JUMPTABLE_START + charCode * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);

            if (!(msbJumpToChar == 255 && lsbJumpToChar == 255)) {
                uint16_t charDataPosition = JUMPTABLE_START + sizeOfJumpTable + ((msbJumpToChar << 8) + lsbJumpToChar);
                drawInternal(self, xPos, yPos, currentCharWidth, textHeight, self->fontData, charDataPosition, charByteSize, color);
            }
            cursorX += currentCharWidth;
        }
    }
}

uint16_t OLED_getStringWidth(OLED* self, const char* text, uint16_t length) {
    uint16_t firstChar = *(self->fontData + FIRST_CHAR_POS);
    uint16_t stringWidth = 0;
    uint16_t maxWidth = 0;

    while (length--) {
        stringWidth += *(self->fontData + JUMPTABLE_START + (text[length] - firstChar) * JUMPTABLE_BYTES + JUMPTABLE_WIDTH);
        if (text[length] == 10) {
            maxWidth = fmax(maxWidth, stringWidth);
            stringWidth = 0;
        }
    }

    return fmax(maxWidth, stringWidth);
}

void OLED_drawString(OLED* self, int16_t xMove, int16_t yMove, const char *stringUser, OLEDDISPLAY_COLOR color) {
    uint16_t lineHeight = *(self->fontData + HEIGHT_POS);
    uint16_t yOffset = 0;
    uint16_t line = 0;

    char *t = (char *) malloc(strlen(stringUser) + 1);
    strcpy(t, stringUser);
    char* rest = t;
    char* textPart;

    while ((textPart = strtok_r(rest, "\n", &rest)) != NULL) {
        uint16_t length = strlen(textPart);
        drawStringInternal(self, xMove, yMove - yOffset + (line++) * lineHeight, textPart, length, OLED_getStringWidth(self, textPart, length), color);
    }

    free(t);
}

void OLED_setFont(OLED* self, const uint8_t* f) {
    self->fontData = f;
}
