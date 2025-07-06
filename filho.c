#include<stdio.h>
#include<stdlib.h>
#include <time.h>

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




int main(void)
{
    // Inicializa o gerador de números aleatórios com uma semente
    srand(time(NULL));  // Faz com que os números mudem a cada execução

    

    return 0;
}