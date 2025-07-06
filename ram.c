#include<stdio.h>
#include<stdlib.h>
#include <time.h>

// Estruturas da Ram
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

// Estruturas dos processos
typedef struct
{
    int numero;    // numero da pagina. vazia - -1
    int modo;      // leitura - 0, escrita - 1 
}Pagina;

typedef struct 
{
    int estado; // bloqueado - 0, pronto - 1
    Pagina pagina_guardada;  // guarda a pagina gerada em caso de page-fault
    Pagina pagina_atual;   // pagina gerada
}Processo;


// Variaveis globais
Ram ram;


// kernel
void alocar_TP(){
    for(int i = 0; i < 128; i++){
        ram.tabela_de_paginas[i] = (Entrada*)malloc(sizeof(Entrada));
        ram.tabela_de_paginas[i]->presente_na_ram = 0;
        ram.tabela_de_paginas[i]->processo = (i / 32) + 1;
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
            printf("\nPage-fault\n");
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

void mostra_paginas_ram(){
    for(int i = 0; i < 128; i++){
        Entrada* entrada = ram.tabela_de_paginas[i];
        if(entrada->presente_na_ram == 1){
            printf("Pagina %d:\n", i);
            printf("processo: %d\tend_virtual: %d\tpresente na ram: %d\tmodificado: %d\treferenciado: %d\tend_fisico: %d\n",
            entrada->processo, entrada->end_virtual, entrada->presente_na_ram, entrada->modificado, entrada->referenciado, entrada->end_fisico);
        }
    }
}

// Funcoes dos processos
// troca estado e guarda a pagina gerada
void bloqueia(Processo processo)
{
    processo.estado = 0;
    processo.pagina_guardada = processo.pagina_atual;
}

void desbloqueia(Processo processo)
{
    processo.estado = 1;
}

void gera_pagina(Processo processo)
{
    if(processo.pagina_guardada.numero != -1){
        processo.pagina_atual = processo.pagina_guardada;
        processo.pagina_guardada.numero = -1;
    }
    else{
        processo.pagina_atual.numero = rand() % 32;
        processo.pagina_atual.modo = rand() % 2;
    }
}

int main(){
    alocar_TP();
    printf("TP alocada\n");

    // Inicializa o gerador de números aleatórios com uma semente
    srand(time(NULL));  // Faz com que os números mudem a cada execução
    
    int pf, modo, pagina;
    for (int i = 0; i < 50; i++){
        pagina = rand() % 32;
        modo = rand() % 2;
        printf("\n\nRodada %d\npagina: %d\tmodo: %d\n\n", i, pagina, modo);
        pf = alocar_entrada(1, pagina, modo);
        if(pf == 1){
            break;
        }
        
        mostra_paginas_ram();
    }

    
    desalocar_TP();
    printf("TP desalocada\n");

    return 0;
}