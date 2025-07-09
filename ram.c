#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Estruturas da Ram
typedef struct
{
    int processo;        // 1-4
    int end_virtual;     // 0-31
    int presente_na_ram; // nao - 0, sim - 1
    int modificado;
    int referenciado;
    int end_fisico; // 0-15

} Entrada;

typedef struct
{
    Entrada *tabela_de_paginas[128]; // vetor de 4 x 32 = 128 entradas
    int vetor_alocadas[16];          // 1 - alocada, 0 vazia
    int contador;                    // conta quantas paginas estao alocadas, maximo 16
} Ram;

// Estruturas de algoritmos

typedef struct
{
    Entrada *entrada;
    int categoria; // 1-4
} Tabela_nru;


// Estruturas dos processos
typedef struct
{
    int numero; // numero da pagina. vazia - -1
    int modo;   // leitura - 0, escrita - 1
} Pagina;

typedef struct
{
    int estado;             // bloqueado - 0, pronto - 1
    Pagina pagina_guardada; // guarda a pagina gerada em caso de page-fault
    Pagina pagina_atual;    // pagina gerada
} Processo;

// Variaveis globais
Ram ram;

// kernel
void alocar_TP()
{
    for (int i = 0; i < 128; i++)
    {
        ram.tabela_de_paginas[i] = (Entrada *)malloc(sizeof(Entrada));
        ram.tabela_de_paginas[i]->presente_na_ram = 0;
        ram.tabela_de_paginas[i]->processo = (i / 32) + 1;
        ram.tabela_de_paginas[i]->end_virtual = i % 32;
    }

    for (int i = 0; i < 16; i++){
        ram.vetor_alocadas[i] = 0;
    }
}

void desalocar_TP()
{
    for (int i = 0; i < 128; i++)
    {
        free(ram.tabela_de_paginas[i]);
    }
}

void remove_pagina(Entrada* entrada){
    // pagina suja indo para o swap
    if (entrada->modificado == 1){
        printf("\nKernel: Página %d do processo %d foi modificada. Simulando cópia para o disco.\n", entrada->end_virtual, entrada->processo);
        entrada->modificado = 0; 
    }
    entrada->presente_na_ram = 0;
    ram.contador--;
    ram.vetor_alocadas[entrada->end_fisico-1] = 0;
    printf("\nKernel: Página %d do processo %d foi removida!\n\n", entrada->end_virtual, entrada->processo);
}

// MMU
Entrada *procura_na_ram(int processo, int end_virtual)
{
    for (int i = 0; i < 128; i++)
    {
        if (ram.tabela_de_paginas[i]->processo == processo && ram.tabela_de_paginas[i]->end_virtual == end_virtual)
        {
            return ram.tabela_de_paginas[i];
        }
    }
}

void zera_bit_R()
{
    for (int i = 0; i < 128; i++)
    {
        Entrada *entrada = ram.tabela_de_paginas[i];
        if (entrada->presente_na_ram == 1)
        {
            entrada->referenciado = 0;
        }
    }
}

int primeiro_vazio_TP(){
    for(int i = 0; i < 16; i++){
        if (ram.vetor_alocadas[i] == 0){
            ram.vetor_alocadas[i] = 1;
            return i+1;
        }
    }
}

// PAGE-FAULT - 1, sem erro - 0
int alocar_entrada(int processo, int end_virtual, int modo)
{
    Entrada *entrada = procura_na_ram(processo, end_virtual);

    // verifica se esta presente na ram
    if (entrada->presente_na_ram == 1)
    {
        entrada->referenciado = 1;
    }
    else
    {
        // quando nao esta na ram, verifica se tem espaço
        if (ram.contador == 16)
        {
            // page-fault
            printf("\nPage-fault\n");
            return 1;
        }
        else
        {
            ram.contador++;
            entrada->presente_na_ram = 1;
            entrada->referenciado = 1;
            entrada->end_fisico = primeiro_vazio_TP();
        }
    }
    // verifica se vai ser modificado
    if (modo == 1)
    {
        entrada->modificado = 1;
    }

    return 0;
}

void mostra_paginas_ram()
{
    for (int i = 0; i < 128; i++)
    {
        Entrada *entrada = ram.tabela_de_paginas[i];
        if (entrada->presente_na_ram == 1)
        {
            printf("Pagina %d:\n", i);
            printf("processo: %d\tend_virtual: %d\tpresente na ram: %d\tmodificado: %d\treferenciado: %d\tend_fisico: %d\n",
                   entrada->processo, entrada->end_virtual, entrada->presente_na_ram, entrada->modificado, entrada->referenciado, entrada->end_fisico);
        }
    }
}

// Algoritmos de substituicao

// NRU
int calcula_categoriaNRU(Tabela_nru valor)
{
    if (valor.entrada->referenciado == 0 && valor.entrada->modificado == 0)
    {
        return 1;
    }
    else if (valor.entrada->referenciado == 0 && valor.entrada->modificado == 1)
    {
        return 2;
    }
    else if (valor.entrada->referenciado == 1 && valor.entrada->modificado == 0)
    {
        return 3;
    }
    else if (valor.entrada->referenciado == 1 && valor.entrada->modificado == 1)
    {
        return 4;
    }
}

void monta_tabelaNRU(Tabela_nru tabela[], int tam)
{
    // monta tabela_nru
    for (int i = 0, j = 0; i < 128; i++)
    {
        Entrada *entrada = ram.tabela_de_paginas[i];
        if (entrada->presente_na_ram == 1)
        {
            tabela[j].entrada = entrada;
            tabela[j].categoria = calcula_categoriaNRU(tabela[j]);
            j++;
        }

    }
}

Entrada *escolhe_paginaNRU(Tabela_nru tabela[], int tam)
{
    for (int i = 0; i < tam; i++)
    {
        if (tabela[i].categoria == 1)
        {
            return tabela[i].entrada;
        }
    }
    for (int i = 0; i < tam; i++)
    {
        if (tabela[i].categoria == 2)
        {
            return tabela[i].entrada;
        }
    }
    for (int i = 0; i < tam; i++)
    {
        if (tabela[i].categoria == 3)
        {
            return tabela[i].entrada;
        }
    }
    for (int i = 0; i < tam; i++)
    {
        if (tabela[i].categoria == 4)
        {
            return tabela[i].entrada;
        }
    }
}

Entrada *NRU()
{
    Tabela_nru tabela[16];
    int retirado;

    monta_tabelaNRU(tabela, 16);
    return escolhe_paginaNRU(tabela, 16);
}

// Second-Chance
typedef struct No_SC No_SC;

typedef struct {
    Entrada* entrada;
    No_SC* prox;
} No_SC;

typedef struct 
{
    No_SC* inicio;
}Fila_SC;

void termina_fila_SC(Fila_SC* fila){
     No_SC* primeiro = (No_SC*)malloc(sizeof(No_SC));
    if (primeiro == NULL) {
        perror("Erro ao alocar no");
        exit(1);
    }

    primeiro->entrada = NULL;
    primeiro->prox = NULL;
    fila->inicio = primeiro;

    No_SC* atual = primeiro;

    for (int i = 1; i < 16; i++) {
        No_SC* novo = (No_SC*)malloc(sizeof(No_SC));
        if (novo == NULL) {
            perror("Erro ao alocar no");
            exit(1);
        }
        novo->entrada = NULL;
        novo->prox = NULL;

        atual->prox = novo;
        atual = novo;
    }

    // fecha a lista circular
    atual->prox = primeiro;
}

Fila_SC* cria_fila_SC(){
    Fila_SC* fila = (Fila_SC*)malloc(sizeof(Fila_SC));
    fila->inicio = NULL;
    termina_fila_SC(fila);
    return fila;
}

void insere_entrada_SC(Fila_SC* fila, Entrada* entrada){
    No_SC* atual = fila->inicio;

    while(atual->entrada != NULL){
        atual = atual->prox;
    }

    atual->entrada = entrada; 
}

Entrada* remove_SC(Fila_SC* fila){
    Entrada* retirada;
    while(fila->inicio->entrada->referenciado == 1){
        fila->inicio->entrada->referenciado = 0;
        fila->inicio = fila->inicio->prox;
    }
    retirada = fila->inicio->entrada;
    retirada->presente_na_ram = 0;
    fila->inicio = fila->inicio->prox;

    return retirada;
}


// Interface para algoritmos usando padrao Strategy
typedef Entrada* (*FuncCriaEstrutura)();
typedef Entrada* (*FuncLiberaEstrutura)();
typedef Entrada* (*FuncNovaEntrada)();
typedef Entrada* (*FuncTerminoRodada)();
typedef Entrada* (*FuncZeroBitsR)();
typedef Entrada* (*FuncSelecionaPagina)();

typedef struct 
{
    FuncCriaEstrutura cria_estrutura;
    FuncSelecionaPagina seleciona_pagina;
}Substituicao;

void escolher_algoritmo(Substituicao* substituicao, int algoritmo){
    if(algoritmo == 1){
        substituicao->seleciona_pagina = NRU;
    }
    else if(algoritmo == 2){
        substituicao->seleciona_pagina = remove_SC;
        substituicao->cria_estrutura = cria_fila_SC;
    }
    else{
        printf("\nEscolha errada\n");
        exit(1);
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
    if (processo.pagina_guardada.numero != -1)
    {
        processo.pagina_atual = processo.pagina_guardada;
        processo.pagina_guardada.numero = -1;
    }
    else
    {
        processo.pagina_atual.numero = rand() % 32;
        processo.pagina_atual.modo = rand() % 2;
    }
}

int main()
{
    Substituicao substituicao;
    escolher_algoritmo(&substituicao, 2);

    // se substituicao != 1
    Fila_SC* fila = cria_fila_SC();

    alocar_TP();
    printf("TP alocada\n");

    // Inicializa o gerador de números aleatórios com uma semente
    srand(time(NULL)); // Faz com que os números mudem a cada execução

    // escalonamento round-robin
    int rodadas = 20;
    int pf, modo, pagina;
    for (int processo = 1; rodadas >= 0; rodadas--, processo++)
    {
        pagina = rand() % 32;
        modo = rand() % 2;
        printf("\n\nRodada %d\npagina: %d\tmodo: %d\n\n", rodadas, pagina, modo);
        pf = alocar_entrada(processo, pagina, modo);
        // se substituicao != 1
        insere_entrada_SC(fila, procura_na_ram(processo, pagina));
        if (pf == 1)
        {
            mostra_paginas_ram();

            // trata page-fault
            // se substituicao != 1
            Entrada* retirada = substituicao.seleciona_pagina(fila);
            remove_pagina(retirada);
            
            mostra_paginas_ram();
            pf = alocar_entrada(processo, pagina, modo);
            insere_entrada_SC(fila, procura_na_ram(processo, pagina));
            printf("\nKernel: Página %d do processo %d foi alocada!\n\n", pagina, processo);
            mostra_paginas_ram();
        }

        //mostra_paginas_ram();

        // acaba 1 rodada
        if (processo == 4)
        {
            processo = 1;
            zera_bit_R();
        }
    }

    
    desalocar_TP();
    printf("TP desalocada\n");

    return 0;
}