#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>

#define NUM_FILHOS 4

// Estruturas da Ram
typedef struct
{
    int processo;        // 1-4
    int end_virtual;     // 0-31
    int presente_na_ram; // nao - 0, sim - 1
    int modificado;
    int referenciado;
    int end_fisico; // 1-16

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
Processo* p = NULL;      // ponteiro para memória compartilhada
Processo processo_local; // struct local em cada processo
pid_t filhos[NUM_FILHOS];
int shmid;
int sinal_recebido = 0;

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
        if(ram.tabela_de_paginas[i] != NULL){
            free(ram.tabela_de_paginas[i]);
            ram.tabela_de_paginas[i] = NULL;
        }
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
typedef struct No_SC {
    Entrada* entrada;
    struct No_SC* prox;
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

void* cria_fila_SC(){
    Fila_SC* fila = (Fila_SC*)malloc(sizeof(Fila_SC));
    fila->inicio = NULL;
    termina_fila_SC(fila);
    return fila;
}

void insere_entrada_SC(void* estrutura, Entrada* entrada){
    Fila_SC* fila = (Fila_SC*)estrutura;

    No_SC* atual = fila->inicio;

    while(atual->entrada != NULL){
        if (atual->entrada == entrada){
            break;
        }
        atual = atual->prox;
    }

    atual->entrada = entrada; 
}

Entrada* remove_SC(void* estrutura){
    Fila_SC* fila = (Fila_SC*)estrutura;

    No_SC* retirado;
    Entrada* retirada;
    while(fila->inicio->entrada->referenciado == 1){
        fila->inicio->entrada->referenciado = 0;
        fila->inicio = fila->inicio->prox;
    }
    retirada = fila->inicio->entrada;
    retirado = fila->inicio;
    fila->inicio = fila->inicio->prox;
    retirado->entrada = NULL;

    return retirada;
}

void imprime_fila_SC(void* estrutura){
    Fila_SC* fila = (Fila_SC*)estrutura;

    No_SC* atual = fila->inicio;
    printf("\nFila Segunda_chance:\n");
    for(int i = 0; i < 16; i++){
        if(atual->entrada == NULL){
            printf("espaco vazio\n");
        }
        else{
            printf("Processo: %d, end_virtual: %d, referenciado: %d\n", atual->entrada->processo, atual->entrada->end_virtual, atual->entrada->referenciado);
        }
        atual = atual->prox;
    }
    printf("\n\tFim da fila\n");
}

void libera_SC(void* estrutura){
    Fila_SC* fila = (Fila_SC*)estrutura;

    No_SC* atual;
    for(int i = 0; i < 16; i++){
        atual = fila->inicio;
        fila->inicio = atual->prox;
        free(atual);
    }
    free(fila);
}

// Working-set (local)
typedef struct 
{
    int processo;
    int* vetor;
}No_WK;

typedef struct{
    No_WK processos[4];
    int k;
}Estrutura_WK;

void* cria_estrutura_WK(int k){
    Estrutura_WK* estrutura = (Estrutura_WK*)malloc(sizeof(Estrutura_WK));
    estrutura->k = k;
    for(int i = 0; i < 4; i++){
        estrutura->processos[i].processo = i+1;
        estrutura->processos[i].vetor = (int*)malloc(sizeof(int)*k);
        for(int j = 0; j < k; j++){
            estrutura->processos[i].vetor[j] = -1;
        }
    }

    return estrutura;
}

void libera_estrutura_WK(void* estrutura){
    Estrutura_WK* removida = (Estrutura_WK*)estrutura;

    for(int i = 0; i < 4; i++){
        if(removida->processos[i].vetor != NULL){
            free(removida->processos[i].vetor);
            removida->processos[i].vetor = NULL;
        }
    }
    free(removida);
}

void remove_velho_WK(int* vetor, int k){
    for(int i = 0; i < (k-1); i++){
        vetor[i] = vetor[i+1];
    }
}

void referencia_WK(int* vetor, int pos, int k){
    int referenciado = vetor[pos];
    int ult_valor;
    int pos_ult_valor;
    // achar o ultimo valor
    for(int i = k-1; i >= pos; i--){
        if(vetor[i] != -1){
            ult_valor = vetor[i];
            pos_ult_valor = i;
            break;
        }
    }
    // coloca o referenciado no final do vetor
    for(int i = 0; i < (pos_ult_valor-pos); i++){
        vetor[pos+i] = vetor[pos+i+1];
    }
    vetor[pos_ult_valor] = referenciado;
}

void insere_entrada_WK(void* estrutura, Entrada* entrada){
    Estrutura_WK* estrutura_wk = (Estrutura_WK*)estrutura;

    int k = estrutura_wk->k;
    int processo = entrada->processo;
    int end_virtual = entrada->end_virtual;
    int* vetor = estrutura_wk->processos[processo-1].vetor;

    for(int i = 0; i < k; i++){
        if(vetor[i] == end_virtual){
            referencia_WK(vetor, i, k);
            return;
        }
        if(vetor[i] == -1 ){
            vetor[i] = end_virtual;
            return;
        }
    }

    remove_velho_WK(vetor, k);

    vetor[k-1] = end_virtual;

}



Entrada* remove_WK(void* estrutura, int processo){
    Estrutura_WK* estrutura_wk = (Estrutura_WK*)estrutura;

    int* vetor = estrutura_wk->processos[processo-1].vetor;
    int k = estrutura_wk->k;
    int esta_no_wk;

    Entrada* retirada;
    // ver se tem entrada do processo fora do WK e na ram
    for(int i = 0; i < 128; i++){
        esta_no_wk = 0;
        if (ram.tabela_de_paginas[i]->processo == processo && ram.tabela_de_paginas[i]->presente_na_ram == 1){
            for(int j = 0; j < k; j++){
                if(vetor[j] == ram.tabela_de_paginas[i]->end_virtual){
                    esta_no_wk = 1;
                    break;
                }
            }
            if(esta_no_wk == 0){
                return ram.tabela_de_paginas[i];
            }
        }
    }
    // remover o mais velho do WK
    retirada = procura_na_ram(processo, vetor[0]);
    remove_velho_WK(vetor, k);
    vetor[k-1] = -1;

    return retirada;
}

void imprime_WK(void* estrutura){
    Estrutura_WK* estrutura_wk = (Estrutura_WK*)estrutura;
    int k = estrutura_wk->k;
    int* lista_processo; 
    printf("\n======= Working Sets =======\n");
    for(int i = 0; i < 4; i++){
        lista_processo = estrutura_wk->processos[i].vetor;
        printf("\nWorkin-set do processo: %d\n", (i+1));
        for(int j = 0; j < k; j++){
            if (lista_processo[j] == -1)
                printf("[ ] ");
            else
                printf("[%02d] ", lista_processo[j]);
        }
    }
    printf("\n");
}

// LRU-anging (local)
typedef struct 
{
    Entrada* entrada;
    unsigned char contador;
}No_LRU;

typedef struct
{
    No_LRU vetor[16];
}Estrutura_LRU;

void* cria_estrutura_LRU(){
    Estrutura_LRU* estrutura = (Estrutura_LRU*)malloc(sizeof(Estrutura_LRU));

    for(int i = 0; i < 16; i++){
        estrutura->vetor[i].contador = 0;
        estrutura->vetor[i].entrada = NULL;
    }

    return estrutura;
}

void insere_entrada_LRU(void* estrutura, Entrada* entrada){
    Estrutura_LRU* estrutura_lru = (Estrutura_LRU*)estrutura;

    for(int i = 0; i < 16; i++){
        if(estrutura_lru->vetor[i].entrada == entrada){
            return;
        }
        if(estrutura_lru->vetor[i].entrada == NULL){
            estrutura_lru->vetor[i].entrada = entrada;
            return;
        }

    }
}

// sempre que for usar criar antes int* tamanho e vetor retornado deve ser liberado apos uso
No_LRU* vetor_processo(Estrutura_LRU* estrutura_lru, int processo, int* tamanho){
    int qtd_entradas = 0;
    No_LRU* v_original = estrutura_lru->vetor;
    // conta entradas do processo (para criar vetor dps)
    for(int i = 0; i < 16; i++){
        if(v_original[i].entrada != NULL && v_original[i].entrada->processo == processo){
            qtd_entradas++;
        }
    }

    // nao acontece
    if(qtd_entradas == 0){
        printf("\nFalha - sem pagina para retirar local\n");
        exit(1);
    }

    // cria vetor dinamico com as entradas do processo
    No_LRU* novo_vetor = (No_LRU*)malloc(sizeof(No_LRU)*qtd_entradas);
    for(int i = 0, j = 0; i < 16; i++){
        if(v_original[i].entrada != NULL && v_original[i].entrada->processo == processo){
            novo_vetor[j] = v_original[i];
            j++;
        }
    }

    *tamanho = qtd_entradas;
    return novo_vetor;

}

unsigned char verifica_menor_contador(No_LRU* novo_vetor, int tamanho){
    unsigned char menor_valor = novo_vetor[0].contador;
    for(int i = 1; i < tamanho; i++){
        if(novo_vetor[i].contador < menor_valor){ 
            menor_valor = novo_vetor[i].contador;
        }
    }

    return menor_valor;
}

Entrada* remove_LRU(void* estrutura, int processo){
    Estrutura_LRU* estrutura_lru = (Estrutura_LRU*)estrutura;

    Entrada* removida;
    int tamanho;
    No_LRU* novo_vetor = vetor_processo(estrutura_lru, processo, &tamanho);
    unsigned char menor_valor = verifica_menor_contador(novo_vetor, tamanho);
    for(int i = 0; i < tamanho; i++){
        if(novo_vetor[i].contador == menor_valor){
            removida =  novo_vetor[i].entrada;
            break;
        }
    }
    for(int i = 0; i < 16; i++){
        if(estrutura_lru->vetor[i].entrada == removida){
            estrutura_lru->vetor[i].entrada = NULL;
            estrutura_lru->vetor[i].contador = 0;
            break;
        }
    }

    free(novo_vetor);

    return removida;

}

void atualiza_estrutura_LRU(void* estrutura){
    Estrutura_LRU* estrutura_lru = (Estrutura_LRU*)estrutura;

    for(int i = 0; i < 16; i++){
        if (estrutura_lru->vetor[i].entrada != NULL) {
            estrutura_lru->vetor[i].contador >>= 1;

            if (estrutura_lru->vetor[i].entrada->referenciado == 1){
                estrutura_lru->vetor[i].contador |= 0x80;  // seta o bit mais significativo
            }
        }
    }
}

void exibe_estrutura_LRU(void* estrutura){
    Estrutura_LRU* estrutura_lru = (Estrutura_LRU*)estrutura;

    No_LRU atual;
    printf("\n\tEstrutura LRU_Anging:\n");
    for(int i = 0; i < 16; i++){
        atual = estrutura_lru->vetor[i];
        if (atual.entrada != NULL){
            printf("processo %d, end_virtual: %d, contador: %u\n",
                    atual.entrada->processo,
                    atual.entrada->end_virtual, 
                    atual.contador);
        }
        else{
            printf("vazio\n");
        }
    }
    printf("\n");
}

void libera_estrutura_LRU(void* estrutura){
    Estrutura_LRU* estrutura_lru = (Estrutura_LRU*)estrutura;

    free(estrutura_lru);
}

// Interface para algoritmos usando padrao Strategy
typedef void* (*FuncCriaEstrutura)();
typedef void (*FuncLiberaEstrutura)();
typedef void (*FuncNovaEntrada)();
typedef void (*FuncTerminoRodada)();  
typedef Entrada* (*FuncSelecionaPagina)();
typedef void (*FuncImprimeEstrutura)();

typedef struct 
{
    int algoritmo;
    FuncCriaEstrutura cria_estrutura;
    FuncSelecionaPagina seleciona_pagina;
    FuncNovaEntrada nova_entrada;
    FuncImprimeEstrutura imprime_estrutura;
    FuncLiberaEstrutura libera_estrutura;
    FuncTerminoRodada termino_rodada;
}Substituicao;

void escolher_algoritmo(Substituicao* substituicao, int algoritmo){
    substituicao->algoritmo =algoritmo;
    if(algoritmo == 1){
        substituicao->seleciona_pagina = NRU;
    }
    else if(algoritmo == 2){
        substituicao->seleciona_pagina = remove_SC;
        substituicao->cria_estrutura = cria_fila_SC;
        substituicao->libera_estrutura = libera_SC;
        substituicao->nova_entrada = insere_entrada_SC;
        substituicao->imprime_estrutura = imprime_fila_SC;
    }
    else if(algoritmo == 3){
        substituicao->seleciona_pagina = remove_WK; //diferente local
        substituicao->cria_estrutura = cria_estrutura_WK; //diferente
        substituicao->libera_estrutura = libera_estrutura_WK;
        substituicao->nova_entrada = insere_entrada_WK;
        substituicao->imprime_estrutura = imprime_WK;
    }
    else if(algoritmo == 4){
        substituicao->seleciona_pagina = remove_LRU; // local
        substituicao->cria_estrutura = cria_estrutura_LRU;
        substituicao->libera_estrutura = libera_estrutura_LRU;
        substituicao->nova_entrada = insere_entrada_LRU;
        substituicao->imprime_estrutura = exibe_estrutura_LRU;
        substituicao->termino_rodada = atualiza_estrutura_LRU;
    }
    else{
        printf("\nEscolha errada\n");
        exit(1);
    }
    
}

// Funcoes dos processos e handlers
void gera_pagina(Processo* proc) {
    
    proc->pagina_atual.numero = rand() % 32;
    proc->pagina_atual.modo = rand() % 2;
    
}

void handler_sigusr1(int sig) {
    gera_pagina(&processo_local);
    *p = processo_local; // sobrescreve memoria compartilhada

    // Avisa o pai que terminou
    kill(getppid(), SIGUSR2);
}

void handler_sigusr2(int sig) {
    // lê a memória compartilhada atualizada
    processo_local = *p;
    sinal_recebido = 1;
}

int main()
{
    int opcao;
    int rodadas;
    printf("\nEscolha um algoritmo: NRU(global) - 1, Segunda-Chance(global) - 2, Workin-Set(local) - 3, LRU-Anging(local) - 4\n");
    scanf("%d", &opcao);
    Substituicao substituicao;
    escolher_algoritmo(&substituicao, opcao);
    printf("\nEscolha o numero de rodadas: \n");
    scanf("%d", &rodadas);

    // cria estrutura
    void* estrutura;
    // se substituicao != 1
    switch (substituicao.algoritmo)
    {
        case 1:
            break;
        
        case 2:
            estrutura = substituicao.cria_estrutura();
            break;
        
        case 3:
            int k;
            printf("\nDiga um tamanho para k(sem ser maior que 5): \n");
            scanf("%d", &k);
            if(k > 5){
                printf("valor maior que 5");
                exit(1);
            }
            estrutura = substituicao.cria_estrutura(k);
            break;
        
        case 4:
            estrutura = substituicao.cria_estrutura();
            break;
        
        default:
            break;
    }
     

    alocar_TP();
    printf("TP alocada\n");

    // Inicializa o gerador de números aleatórios com uma semente
    srand(time(NULL)); // Faz com que os números mudem a cada execução

    // Criar memória compartilhada para um Processo
    shmid = shmget(IPC_PRIVATE, sizeof(Processo), IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }
    p = (Processo*)shmat(shmid, NULL, 0);
    if (p == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    // Inicializa a memória compartilhada
    processo_local.estado = 1;
    processo_local.pagina_guardada.numero = -1;
    processo_local.pagina_atual.numero = -1;
    *p = processo_local;

    // Cria filhos e registra handlers
    for (int i = 0; i < NUM_FILHOS; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Filho
            srand(time(NULL) ^ (getpid()<<16));
            signal(SIGUSR1, handler_sigusr1);

            while (1) {
                pause();  // espera sinal do pai
            }
            exit(0);
        } else {
            filhos[i] = pid;
        }
    }

    // Pai registra handler para SIGUSR2 
    signal(SIGUSR2, handler_sigusr2);

    // escalonamento round-robin
    int pf, modo, pagina;
    for (int processo = 1; rodadas >= 0; processo++)
    {
        kill(filhos[processo-1], SIGUSR1);
         // espera até o sinal chegar
        while (!sinal_recebido) {
            pause();
        }
        sinal_recebido = 0;

        modo = processo_local.pagina_atual.modo;
        pagina = processo_local.pagina_atual.numero;

        printf("\n\nRodada %d\npagina: %d\tmodo: %d\n\n", rodadas, pagina, modo);
        if(rodadas <= 4){
            printf("\n2\n");
        }
        pf = alocar_entrada(processo, pagina, modo);
        if (pf == 1)
        {
            printf("\n1\n");
            mostra_paginas_ram();
            if(substituicao.algoritmo != 1){
                substituicao.imprime_estrutura(estrutura);
            }
            // trata page-fault
            Entrada* retirada;
            switch (substituicao.algoritmo)
            {
                case 1:
                    retirada = substituicao.seleciona_pagina();
                    break;
                
                case 2:
                    retirada = substituicao.seleciona_pagina(estrutura);
                    break;
                
                case 3:
                    retirada = substituicao.seleciona_pagina(estrutura, processo);
                    break;
                
                case 4:
                    retirada = substituicao.seleciona_pagina(estrutura, processo);
                    break;
                
                default:
                    break;
            }

            remove_pagina(retirada);

            if(substituicao.algoritmo != 1){
                substituicao.imprime_estrutura(estrutura);
            }
            //mostra_paginas_ram();

            pf = alocar_entrada(processo, pagina, modo);
            
        }
        if(substituicao.algoritmo != 1){
            substituicao.nova_entrada(estrutura, procura_na_ram(processo, pagina));
        }
        printf("\nKernel: Página %d do processo %d foi alocada!\n\n", pagina, processo);
        //mostra_paginas_ram();
        if(substituicao.algoritmo != 1){
                substituicao.imprime_estrutura(estrutura);
        }

        //mostra_paginas_ram();

        // acaba 1 rodada
        if (processo == 4)
        {
            if(substituicao.algoritmo == 4){
                substituicao.termino_rodada(estrutura);
            }
            zera_bit_R();
            processo = 0;
            rodadas--;
        }
    }

    // Mata filhos e limpa
    for (int i = 0; i < NUM_FILHOS; i++) {
        kill(filhos[i], SIGKILL);
        waitpid(filhos[i], NULL, 0);
    }

    // Libera Shmem
    shmdt(p);
    shmctl(shmid, IPC_RMID, NULL); 

    if(substituicao.algoritmo != 1){
        substituicao.libera_estrutura(estrutura);
        printf("estrutura desalocada\n");
    }
    desalocar_TP();
    printf("TP desalocada\n");

    return 0;
}