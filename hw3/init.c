#define _GNU_SOURCE
#include "init.h"
#include "scheduler.h"
#include "thread.h"
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <unistd.h>

void SIGALRM_Handler(int signo) {
    if (ReadyQueue.queueCount == 0 && pCurrentThread == NULL) {

        return;
    }
    Thread *curThread = pCurrentThread;
    Thread *newThread = ReadyQueue.pHead;
    int curpid = -1;
    int newpid = -1;
    if (curThread != NULL && curThread->status == THREAD_STATUS_RUN) {
        curpid = curThread->pid;
        curThread->status = THREAD_STATUS_READY;
        if (ReadyQueue.queueCount == 0) {
            ReadyQueue.pHead = curThread;
            ReadyQueue.pTail = ReadyQueue.pHead;
        } else {
            curThread->pPrev = ReadyQueue.pTail;
            ReadyQueue.pTail->pNext = curThread;
            ReadyQueue.pTail = curThread;
        }
        ReadyQueue.queueCount++;
    } else if (curThread != NULL && curThread->status == THREAD_STATUS_ZOMBIE) {
        curpid = curThread->pid;
    }
    if (newThread != NULL && newThread->status == THREAD_STATUS_READY) {
        newpid = newThread->pid;
        newThread->status = THREAD_STATUS_RUN;
        ReadyQueue.pHead = ReadyQueue.pHead->pNext;
        if (ReadyQueue.pHead != NULL) {
            ReadyQueue.pHead->pPrev = NULL;
        }
        newThread->pNext = NULL;
        ReadyQueue.queueCount--;
        pCurrentThread = newThread;
    }
    __ContextSwitch(curpid, newpid);
}
void SIGCHLD_Handler(int signo) { return; }
void Init(void) {

    signal(SIGALRM, SIGALRM_Handler);
    struct sigaction sigact;
    sigact.sa_flags = SA_NOCLDSTOP;
    sigact.sa_handler = SIGCHLD_Handler;
    sigaction(SIGCHLD, &sigact, NULL);
}
