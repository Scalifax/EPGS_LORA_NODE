


#include "ng_command.h"
#include "ng_util.h"
#include "epgs_wrapper.h"


NgCommand *ng_create_command(char* _Name, char* _Alternative, char* _Version) {
	struct _ng_command *p_command = (struct _ng_command*)ng_malloc(sizeof(struct _ng_command));

	p_command->Name = (char*)ng_malloc(sizeof(char)*(ng_strlen(_Name)+1));
	ng_strcpy(p_command->Name, _Name);

	p_command->Alternative = (char*)ng_malloc(sizeof(char)*(ng_strlen(_Alternative)+1));
	ng_strcpy(p_command->Alternative, _Alternative);

	p_command->Version = (char*)ng_malloc(sizeof(char)*(ng_strlen(_Version)+1));
	ng_strcpy(p_command->Version, _Version);

	p_command->NoA=0;
	//p_command->Arguments = (struct _ng_arguments**)ng_malloc(sizeof(struct _ng_arguments*));
	p_command->Arguments = NULL;

	return p_command;
}

void ng_destroy_command(NgCommand** ngCommand) {

    if (!ngCommand || !(*ngCommand))
        return;

    // Libera os argumentos se existirem
    if ((*ngCommand)->NoA > 0 && (*ngCommand)->Arguments) {
        for (unsigned int i = 0; i < (*ngCommand)->NoA; i++) {
            if ((*ngCommand)->Arguments[i]) {
                // Libera os elementos de cada argumento, se existirem
                if ((*ngCommand)->Arguments[i]->NoE > 0 && (*ngCommand)->Arguments[i]->Elements) {
                    for (unsigned int j = 0; j < (*ngCommand)->Arguments[i]->NoE; j++) {
                        if ((*ngCommand)->Arguments[i]->Elements[j]) {
                            // Libera cada elemento se foi alocado dinamicamente
                            ng_free((*ngCommand)->Arguments[i]->Elements[j]);
                            (*ngCommand)->Arguments[i]->Elements[j] = NULL;
                        }
                    }
                    // Libera o array de elementos
                    ng_free((*ngCommand)->Arguments[i]->Elements);
                    (*ngCommand)->Arguments[i]->Elements = NULL;
                }
                // Libera cada argumento
                ng_free((*ngCommand)->Arguments[i]);
                (*ngCommand)->Arguments[i] = NULL;
            }
        }
        // Libera o array de argumentos
        ng_free((*ngCommand)->Arguments);
        (*ngCommand)->Arguments = NULL;
    }

    // Libera os campos Name, Alternative e Version
    if ((*ngCommand)->Name) {
        ng_free((*ngCommand)->Name);
        (*ngCommand)->Name = NULL;
    }
    if ((*ngCommand)->Alternative) {
        ng_free((*ngCommand)->Alternative);
        (*ngCommand)->Alternative = NULL;
    }
    if ((*ngCommand)->Version) {
        ng_free((*ngCommand)->Version);
        (*ngCommand)->Version = NULL;
    }

    // Libera a estrutura NgCommand em si
    ng_free((*ngCommand));
    (*ngCommand) = NULL;
}

/*
void ng_destroy_command(NgCommand** ngCommand) {

    if (!ngCommand || !(*ngCommand))
        return;

    if ((*ngCommand)->NoA > 0 && (*ngCommand)->Arguments) {
        unsigned int i = 0;
        unsigned int j = 0;
        for (i = 0; i < (*ngCommand)->NoA; i++) {
            if ((*ngCommand)->Arguments[i]) {
                if ((*ngCommand)->Arguments[i]->NoE > 0 && (*ngCommand)->Arguments[i]->Elements) {
                    for (j = 0; j < (*ngCommand)->Arguments[i]->NoE; j++) {
                        if ((*ngCommand)->Arguments[i]->Elements[j]) {
                            // Only free if the element was dynamically allocated
                            // If we cannot be sure, avoid freeing to prevent crashes
                            // ng_free((*ngCommand)->Arguments[i]->Elements[j]);
                            (*ngCommand)->Arguments[i]->Elements[j] = NULL;
                        }
                    }
                    ng_free((*ngCommand)->Arguments[i]->Elements);
                    (*ngCommand)->Arguments[i]->Elements = NULL;
                }
                ng_free((*ngCommand)->Arguments[i]);
                (*ngCommand)->Arguments[i] = NULL;
            }
        }
        ng_free((*ngCommand)->Arguments);
        (*ngCommand)->Arguments = NULL;
    }

    // Avoid freeing these if they were not dynamically allocated
    // Comment out the ng_free calls if necessary
    // if ((*ngCommand)->Name) {
    //     ng_free((*ngCommand)->Name);
    //     (*ngCommand)->Name = NULL;
    // }
    // if ((*ngCommand)->Alternative) {
    //     ng_free((*ngCommand)->Alternative);
    //     (*ngCommand)->Alternative = NULL;
    // }
    // if ((*ngCommand)->Version) {
    //     ng_free((*ngCommand)->Version);
    //     (*ngCommand)->Version = NULL;
    // }

    // Set pointers to NULL to avoid dangling references
    (*ngCommand)->Name = NULL;
    (*ngCommand)->Alternative = NULL;
    (*ngCommand)->Version = NULL;

    ng_free((*ngCommand));
    (*ngCommand) = NULL;
}
*/

// Allocates and add a string vector on Arguments container
void NewArgument(struct _ng_command *ngCommand, int _Size) {

	ngCommand->Arguments = (struct _ng_arguments**) ng_realloc(ngCommand->Arguments, (ngCommand->NoA + 1) * sizeof(struct _ng_arguments*));

	ngCommand->Arguments[ngCommand->NoA] = (struct _ng_arguments*) ng_malloc(sizeof(struct _ng_arguments));
	ngCommand->Arguments[ngCommand->NoA]->NoE = _Size;
	ngCommand->Arguments[ngCommand->NoA]->Elements = (char**)ng_malloc(sizeof(char*) * _Size);

	ngCommand->NoA++;
}

// Get argument
int GetArgument(struct _ng_command *ngCommand, unsigned int _Index, struct _ng_arguments** _Argument) {

	if (_Index < ngCommand->NoA)
	{
		unsigned int i=0;
		struct _ng_arguments* Argument = (struct _ng_arguments*) ng_malloc(sizeof(struct _ng_arguments));
		Argument->Elements = (char**)ng_malloc(sizeof(char*)*ngCommand->Arguments[_Index]->NoE);
		Argument->NoE = ngCommand->Arguments[_Index]->NoE;
		
		for (i=0; i<ngCommand->Arguments[_Index]->NoE; i++)
		{
			Argument->Elements[i] = (char*)ng_malloc(sizeof(char)*(ng_strlen(ngCommand->Arguments[_Index]->Elements[i] +1)));
			ng_strcpy(Argument->Elements[i], ngCommand->Arguments[_Index]->Elements[i]);
		}

		*_Argument = Argument;
		return NG_OK;
	}

	return NG_ERROR;
}

// Get number of arguments
int GetNumberofArguments(struct _ng_command *ngCommand, unsigned int *_Number) {

	*_Number = ngCommand->NoA;
	return NG_OK;
}

// Get number of elements in a certain argument
int GetNumberofArgumentElements(struct _ng_command *ngCommand, unsigned int _Index, unsigned int *_Number) {
	if (_Index < ngCommand->NoA)
	{
		*_Number=ngCommand->Arguments[_Index]->NoE;

		return NG_OK;
	}

	return NG_ERROR;
}

// Set an element at an Argument
int SetArgumentElement(struct _ng_command *ngCommand, unsigned int _Index, unsigned int _Element, char* _Value) {

	if (_Index < ngCommand->NoA)
	{
		if (_Element < ngCommand->Arguments[_Index]->NoE)
		{
			ngCommand->Arguments[_Index]->Elements[_Element] = (char*) ng_malloc(sizeof(char)*(ng_strlen(_Value)+1));
			ng_strcpy(ngCommand->Arguments[_Index]->Elements[_Element],_Value);

			return NG_OK;
		}
		else
		{
			return NG_ERROR;
		}
	}

	return NG_ERROR;
}

// Get an element at an Argument
int GetArgumentElement(struct _ng_command *ngCommand, unsigned int _Index, unsigned int _Element, char** _Value) {
	if (_Index < ngCommand->NoA)
	{
		if (_Element < ngCommand->Arguments[_Index]->NoE)
		{
			*_Value = (char*) ng_malloc(sizeof(char)*(ng_strlen(ngCommand->Arguments[_Index]->Elements[_Element])+1));
			ng_strcpy(*_Value, ngCommand->Arguments[_Index]->Elements[_Element]);

			return NG_OK;
		}

		return NG_ERROR;
	}

	return NG_ERROR;
}

char* ConvertNgCommandToString(struct _ng_command *CL) {
	char NG[4]="ng ";
	char SP[2]=" ";
	char NL[2]="\n";
	char OA[3]="[ ";
	char CA[2]="]";
	char OV[3]="< ";
	char CV[2]=">";
	unsigned int    _Size;

	char* output = (char*)ng_calloc(sizeof(char), 512);

	ng_sprintf(output + ng_strlen(output), "%s%s%s%s%s%s", NG, CL->Name, SP, CL->Alternative, SP, CL->Version);

	if(CL->NoA > 0)
	{
		ng_sprintf(output + ng_strlen(output), "%s%s", SP, OA);

		// Run over the number of arguments
		unsigned int i=0;
		for (i=0; i< CL->NoA; i++)
		{

			ng_sprintf(output + ng_strlen(output), "%s", OV);

			_Size=CL->Arguments[i]->NoE;

			ng_sprintf(output + ng_strlen(output), "%u%ss ", _Size, SP);


			unsigned int j=0;
			for (j=0; j<CL->Arguments[i]->NoE; j++)
			{
				ng_sprintf(output + ng_strlen(output), "%s%s", CL->Arguments[i]->Elements[j],SP);
			}

			ng_sprintf(output + ng_strlen(output), "%s%s", CV, SP);

		}

		ng_sprintf(output + ng_strlen(output), "%s%s", CA, NL);
	}

	if(CL->NoA == 0)
	{
		ng_sprintf(output + ng_strlen(output), "%s", NL);
	}

	return output;
}

NgCommand *ConvertStringToNgCommand(char *inputStr){
	int				Size;
	unsigned int	Argument_Number=0;
	NgCommand *CL = NULL;
	
	char* auxStr = (char*)ng_malloc(sizeof(char)*(ng_strlen(inputStr)+1));
	ng_strcpy(auxStr, inputStr);

	char *end_str;
	char* token = strtok_r(auxStr, " ", &end_str);
	

	if (ng_strcmp(token, "ng")==0)
	{
		token = strtok_r(NULL, " ", &end_str);
		char* name = (char*)ng_malloc(sizeof(char)*(ng_strlen(token)+1));
		ng_strcpy(name, token);
		token = strtok_r(NULL, " ", &end_str);
		char *alternative = (char*)ng_malloc(sizeof(char)*(ng_strlen(token)+1));
		ng_strcpy(alternative, token);
		token = strtok_r(NULL, " ", &end_str);
		char *version = (char*)ng_malloc(sizeof(char)*(ng_strlen(token)+1));
		ng_strcpy(version, token);
		
		CL = ng_create_command(name, alternative, version);
		
		ng_free(name);
		ng_free(alternative);
		ng_free(version);

		// Reads the [
		token = strtok_r(NULL, " ", &end_str);

		if (token && ng_strcmp(token, "[")==0 && ng_strcmp(CL->Name, "")!=0 && ng_strcmp(CL->Alternative, "")!=0 && ng_strcmp(CL->Version, "")!=0 && ng_strcmp(token, "[<")!=0)
		{
			while(token)
			{
				token = strtok_r(NULL, " ", &end_str); // Reads the <

				if (token && ng_strcmp(token, "<")==0 && ng_strcmp(token, "<1")!=0 && ng_strcmp(token, "<2")!=0  && ng_strcmp(token, "<3")!=0 && ng_strcmp(token, "]")!=0)
				{
					token = strtok_r(NULL, " ", &end_str);  // Reads the number

					if(token) {
					
						Size = ng_atoi(token);

						if (Size > 0)
						{
							// New argument
							NewArgument(CL, Size);

							token = strtok_r(NULL, " ", &end_str);  // Reads the type

							if (token && (ng_strcmp(token, "s")==0 || ng_strcmp(token, "h")==0 || ng_strcmp(token, "i")==0)
								&& (ng_strcmp(token, "1s")!=0 && ng_strcmp(token, "2s")!=0 && ng_strcmp(token, "3s")!=0) )
							{
								int i=0;
								for (i=0; i < Size; i++)
								{
									token = strtok_r(NULL, " ", &end_str);  // Reads the value

									if (token && ng_strcmp(token, "")!=0 && ng_strcmp(token, ">")!=0 && ng_strcmp(token, "<")!=0 && ng_strcmp(token, "]")!=0 && ng_strcmp(token, " ")!=0 )
									{
										// New element
										SetArgumentElement(CL, Argument_Number, i, token);
									}
									else
									{
										break;
									}
								}

								token = strtok_r(NULL, " ", &end_str); // Reads the >
							}
							else
							{
								break;
							}
						}
					}
				}
				else
				{
					break;
				}

				Argument_Number++;
			}
		}
	}

	token = strtok_r(NULL, " ", &end_str); // Reads the ]
	
	ng_free(auxStr);

	return CL;
}
