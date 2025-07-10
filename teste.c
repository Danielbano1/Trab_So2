#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/shm.h>

#define NUM_FILHOS 4

typedef struct {
    int numero;    // número da página. -1 indica vazia
    int modo;      // leitura - 0, escrita - 1 
} Pagina;

typedef struct {
    int estado;               // bloqueado - 0, pronto - 1
    Pagina pagina_guardada;   // guarda página em caso de page-fault
    Pagina pagina_atual;      // página atual
} Processo;

// Variáveis globais
Processo* p = NULL;      // ponteiro para memória compartilhada
Processo processo_local; // struct local em cada processo
pid_t filhos[NUM_FILHOS];
int shmid;

// Funções para alterar Processo (passando ponteiro para atualizar realmente)
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
}

int main() {
    srand(time(NULL));

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
            printf("Filho %d: pronto e aguardando sinais.\n", getpid());

            while (1) {
                pause();  // espera sinal do pai
            }
            exit(0);
        } else {
            filhos[i] = pid;
        }
    }

    // Pai registra handler para SIGUSR2 (sinal de filho pronto)
    signal(SIGUSR2, handler_sigusr2);

    printf("Pai %d: filhos criados. Iniciando escalonamento round-robin.\n", getpid());

    int rodadas = 40;
    int pf, modo, pagina;
    for (int processo = 1; rodadas >= 0; rodadas--, processo++){
        kill(filhos[processo-1], SIGUSR1);
        pause();
        modo = processo_local.pagina_atual.modo;
        pagina = processo_local.pagina_atual.numero;
        printf("\n\nRodada %d\npagina: %d\tmodo: %d\n\n", rodadas, pagina, modo);

        // acaba 1 rodada
        if (processo == 4)
        {
            processo = 0;
        }
    }

    // Mata filhos e limpa
    for (int i = 0; i < NUM_FILHOS; i++) {
        kill(filhos[i], SIGKILL);
        waitpid(filhos[i], NULL, 0);
    }

    shmdt(p);
    shmctl(shmid, IPC_RMID, NULL);

    printf("Pai finalizando.\n");

    return 0;
}
    