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
            // page-fault com substituicao de pagina
            printf("\nPage-fault com substituicao de pagina\n");
            return 1;
        }
        else
        {
            // page-fault
            ram.contador++;
            entrada->presente_na_ram = 1;
            entrada->referenciado = 1;
            entrada->end_fisico = ram.contador;
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

    monta_tabelaNRU(tabela, 16);
    return escolhe_paginaNRU(tabela, 16);
}

// Funcoes dos processos
// troca estado e guarda a pagina gerada

void start_processos(Processo lista_processos[], int tam){
    for (int i = 0; i < tam; i++){
        lista_processos[i].estado = 1;
    }
}

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
    alocar_TP();
    printf("TP alocada\n");

    // Inicializa o gerador de números aleatórios com uma semente
    srand(time(NULL)); // Faz com que os números mudem a cada execução

    // Cria processos
    Processo lista_processos[4];
    start_processos(lista_processos, 4);

    // escalonamento round-robin
    int rodadas = 20;
    int pf, modo, pagina;
    Processo proc;

    for (int processo = 1; rodadas >= 0; rodadas--, processo++)
    {
        proc = lista_processos[processo-1];
    
        pagina = proc.pagina_atual.numero;
        modo = proc.pagina_atual.modo;
        gera_pagina(proc);

        printf("\n\nRodada %d\npagina: %d\tmodo: %d\n\n", rodadas, pagina, modo);
        pf = alocar_entrada(processo, pagina, modo);
        if (pf == 1)
        {
            mostra_paginas_ram();

            // trata page-fault com subsituicao
            Entrada* retirada = NRU();
            remove_pagina(retirada);
            
            mostra_paginas_ram();
        }

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