#include "semaphore.h"
#include "scheduler.h"
#include "thread.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
SemaphoreTblEnt pSemaphoreTblEnt[MAX_SEMAPHORE_NUM];

int thread_sem_open(char *name, int count) {
    int idx = -1;
    for (int i = 0; i < MAX_SEMAPHORE_NUM; i++) {
        if (pSemaphoreTblEnt[i].bUsed != 0 &&
            strcmp(name, pSemaphoreTblEnt[i].name) == 0) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        for (int i = 0; i < MAX_SEMAPHORE_NUM; i++) {
            if (pSemaphoreTblEnt[i].bUsed == 0) {
                idx = i;
                break;
            }
        }
        strcpy(pSemaphoreTblEnt[idx].name, name);
        pSemaphoreTblEnt[idx].bUsed = 1;
        Semaphore *sem = (Semaphore *)malloc(sizeof(Semaphore));
        sem->count = count;
        sem->waitingQueue.pHead = NULL;
        sem->waitingQueue.pTail = NULL;
        sem->waitingQueue.queueCount = 0;
        pSemaphoreTblEnt[idx].pSemaphore = sem;
    }
    return idx;
}

int thread_sem_wait(int semid) {
    if (pSemaphoreTblEnt[semid].bUsed == 0) {
        return -1;
    }

    if (pSemaphoreTblEnt[semid].pSemaphore->count > 0) {
        pSemaphoreTblEnt[semid].pSemaphore->count--;
        return 0;
    }
    if (pSemaphoreTblEnt[semid].pSemaphore->count <= 0) {

        Thread *blocked = pCurrentThread;
        Thread *toStart = ReadyQueue.pHead;
        if (blocked == NULL) {
            return -1;
        }
        int blocked_pid = -1;
        int toStart_pid = -1;
        if (toStart != NULL) {
            pSemaphoreTblEnt[semid].pSemaphore->count--;
            blocked_pid = blocked->pid;
            toStart_pid = toStart->pid;

            ReadyQueue.pHead = ReadyQueue.pHead->pNext;
            toStart->pNext = NULL;
            if (ReadyQueue.pHead != NULL) {
                ReadyQueue.pHead->pPrev = NULL;
            }
            toStart->status = THREAD_STATUS_RUN;
            ReadyQueue.queueCount--;
            pCurrentThread = toStart;

            if (pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount >
                0) {
                blocked->pPrev =
                    pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail->pNext =
                    blocked;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail =
                    blocked;
                blocked->pNext = NULL;
            } else {
                blocked->pNext = NULL;
                blocked->pPrev = NULL;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead =
                    blocked;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail =
                    blocked;
            }
            pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount++;
            blocked->status = THREAD_STATUS_WAIT;
            kill(toStart_pid, SIGCONT);
            kill(blocked_pid, SIGSTOP);
        } else {
            pSemaphoreTblEnt[semid].pSemaphore->count--;
            if (pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount >
                0) {
                blocked->pPrev =
                    pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail->pNext =
                    blocked;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail =
                    blocked;
            } else {
                blocked->pNext = NULL;
                blocked->pPrev = NULL;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead =
                    blocked;
                pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pTail =
                    blocked;
            }
            pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount++;
            blocked->status = THREAD_STATUS_WAIT;
            kill(blocked_pid, SIGSTOP);
        }
    }

    return 0;
}

int thread_sem_post(int semid) {
    if (pSemaphoreTblEnt[semid].bUsed == 0) {
        return -1;
    }

    pSemaphoreTblEnt[semid].pSemaphore->count++;
    Thread *toReady = pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead;
    if (pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount > 0 &&
        toReady != NULL) {
        pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead =
            pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead->pNext;
        if (pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead != NULL) {
            pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.pHead->pPrev =
                NULL;
        }
        toReady->pNext = NULL;
        toReady->pPrev = NULL;
        pSemaphoreTblEnt[semid].pSemaphore->waitingQueue.queueCount--;

        if (ReadyQueue.queueCount > 0) {
            toReady->pPrev = ReadyQueue.pTail;
            ReadyQueue.pTail->pNext = toReady;
            ReadyQueue.pTail = toReady;
        } else {
            ReadyQueue.pHead = toReady;
            ReadyQueue.pTail = toReady;
        }
        toReady->status = THREAD_STATUS_READY;
        ReadyQueue.queueCount++;
        return 0;
    } else {
        return -1;
    }
}

int thread_sem_close(int semid) {
    if (pSemaphoreTblEnt[semid].bUsed == 0) {
        return -1;
    }
    memset(pSemaphoreTblEnt[semid].name, '\0', SEM_NAME_MAX);
    pSemaphoreTblEnt[semid].bUsed = 0;
    free(pSemaphoreTblEnt[semid].pSemaphore);
    pSemaphoreTblEnt[semid].pSemaphore = NULL;
    return 0;
}
