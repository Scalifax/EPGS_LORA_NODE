# EPGS - Nó Embarcado para Rede Nova Genesis

![Versão](https://img.shields.io/badge/versão-1.1.0-green.svg)
![Status](https://img.shields.io/badge/status-Em%20Desenvolvimento-yellow.svg)

## Introdução

Bem-vindo ao **EPGS** (Embarked Proxy Gateway Service)! Este programa é um nó de rede que pode se comunicar com a **Nova Genesis**, projetado para operar de maneira embarcada no dispositivo **Heltec LoRa 32 v2**. Ele utiliza o **ESP-IDF** como compilador, e esta versão do EPGS foi desenvolvida para o projeto **Estação Genesis**, uma estação meteorológica avançada que coleta dados climáticos e realiza previsões do tempo.

O EPGS facilita a comunicação IoT da estação, integrando uma biblioteca RS485 personalizada para leitura de sensores climáticos, utilizando o módulo PCNT do ESP32 para contar sensores de pulso, além de contar com um display OLED SSD1306 paara exibição e um módulo LoRa para a transmissão física dos dados.

## Características

- **Operação Embarcada**: Desenvolvido para o dispositivo Heltec LoRa 32 v2.
- **Compatibilidade com Nova Genesis**: Integrado à arquitetura de rede Nova Genesis, porém desenvolvido de maneira separada ao projeto principal.
- **Comunicação RS485**: Biblioteca RS485 programada manualmente para leitura eficiente de sensores climáticos.
- **Módulo PCNT do ESP32**: Utilização do módulo PCNT para contagem de pulsos e gerenciamento de sensores desse tipo.
- **Display OLED SSD1306**: Interface visual para monitoramento durante a configuração da rede.
- **Transmissão LoRa**: Comunicação física robusta e de longo alcance utilizando LoRa.
- **Baixo Consumo de Energia**: Otimizado para operações eficientes em dispositivos embarcados.

## Tecnologias Utilizadas

- **Heltec LoRa 32 v2**: Plataforma de hardware embarcado baseada no ESP32.
- **ESP-IDF**: Framework de desenvolvimento oficial para ESP32.
- **RS485**: Protocolo de comunicação serial para conexão de sensores climáticos.
- **Módulo PCNT do ESP32**: Contador de pulsos para leitura de sensores.
- **Display OLED SSD1306**: Tela de visualização para feedback e configuração.
- **LoRa**: Tecnologia de comunicação sem fio de longo alcance e baixa potência.
- **Nova Genesis**: Arquitetura de rede alternativa ao TCP/IP, projetada pelo prof. Alberti, também é chamada de "a internet do futuro".
