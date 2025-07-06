#include<stdio.h>
#include<stdlib.h>

typedef struct
{
    int processo; // 1-4
    int end_virtual; // 0-31
    int presente_na_ram; // nao - 0, sim - 1  
    int modificado;   
    int referenciado;  
    int end_fisico; // 0-15

}Entrada;

typedef struct
{
    Entrada* tabela_de_paginas[128];  // vetor de 4 x 32 = 128 entradas
    int contador;  // conta quantas paginas estao alocadas, maximo 16    
}Ram;

Ram ram;

void alocar_TP(){
    for(int i = 0; i < 128; i++){
        ram.tabela_de_paginas[i] = (Entrada*)malloc(sizeof(Entrada));
        ram.tabela_de_paginas[i]->presente_na_ram = 0;
        ram.tabela_de_paginas[i]->processo = i / 32;
        ram.tabela_de_paginas[i]->end_virtual = i % 32;
    }
}

void desalocar_TP(){
     for(int i = 0; i < 128; i++){
        free(ram.tabela_de_paginas[i]);
    }
}

// MMU
Entrada* procura_na_ram(int processo, int end_virtual)
{
    for (int i = 0; i < 128; i++){
        if (ram.tabela_de_paginas[i]->processo == processo && ram.tabela_de_paginas[i]->end_virtual == end_virtual){
            return ram.tabela_de_paginas[i];
        }
    }
}


int alocar_entrada(int processo, int end_virtual, int modo)
{
    Entrada* entrada = procura_na_ram(processo, end_virtual);

    // verifica se esta presente na ram 
    if (entrada->presente_na_ram == 1){
        entrada->referenciado = 1;
    }
    else{
        // quando nao esta na ram, verifica se tem espaço
        if (ram.contador == 16){ 
            // page-fault
            return 1;
        }
        else{
            ram.contador++;
            entrada->presente_na_ram = 1;
            entrada->referenciado = 1;
            entrada->end_fisico = ram.contador;
        }
    }
    // verifica se vai ser modificado
    if (modo == 1){
        entrada->modificado = 1;        
    }

    return 0;
}

int main(){
    alocar_TP();
    desalocar_TP();

    return 0;
}