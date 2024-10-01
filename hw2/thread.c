#define _GNU_SOURCE
#include "thread.h"
#include "init.h"
#include "scheduler.h"
#include <stdio.h>
#include <stdlib.h>

ThreadQueue ReadyQueue;
ThreadQueue WaitingQueue;
ThreadTblEnt pThreadTbEnt[MAX_THREAD_NUM];
Thread *pCurrentThread; // Running 상태의 Thread를 가리키는 변수

const int STACK_SIZE = 1024 * 64;

int thread_create(thread_t *thread, thread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg) {
    char *pstack;
    pstack = malloc(STACK_SIZE);
    if (pstack == NULL) {
        return -1;
    }
    int pid;
    int flags = SIGCHLD | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_VM;
    pid = clone(*(start_routine), (char *)pstack + STACK_SIZE, flags, arg);
    kill(pid, SIGSTOP);
    Thread *TCB = malloc(sizeof(Thread));
    TCB->pid = pid;
    TCB->stackSize = STACK_SIZE;
    TCB->stackAddr = pstack;
    TCB->pNext = NULL;
    TCB->pPrev = NULL;
    TCB->cpu_time = 0.0;
    TCB->status = THREAD_STATUS_READY;

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].bUsed == 0) {
            pThreadTbEnt[i].pThread = TCB;
            pThreadTbEnt[i].bUsed = 1;
            *thread = i;
            break;
        }
    }
    if (ReadyQueue.queueCount > 0) {
        TCB->pPrev = ReadyQueue.pTail;
        ReadyQueue.pTail->pNext = TCB;
        ReadyQueue.pTail = TCB;
    } else {
        ReadyQueue.pHead = TCB;
        ReadyQueue.pTail = TCB;
    }
    ReadyQueue.queueCount++;
    return 0;
}

int thread_suspend(thread_t tid) {
    if (tid >= MAX_THREAD_NUM)
        return -1;

    Thread *target = pThreadTbEnt[tid].pThread;

    if (target == NULL ||
        (target != NULL && target->status != THREAD_STATUS_READY)) {
        return -1;
    }

    Thread *prev_target = target->pPrev;
    Thread *next_target = target->pNext;

    if (ReadyQueue.pHead == target) {
        ReadyQueue.pHead = next_target;
    }
    if (ReadyQueue.pTail == target) {
        ReadyQueue.pTail = prev_target;
    }

    if (prev_target != NULL)
        prev_target->pNext = next_target;
    if (next_target != NULL)
        next_target->pPrev = prev_target;

    target->pPrev = NULL;
    target->pNext = NULL;
    target->status = THREAD_STATUS_WAIT;
    ReadyQueue.queueCount--;

    if (WaitingQueue.queueCount > 0) {
        target->pPrev = WaitingQueue.pTail;
        WaitingQueue.pTail->pNext = target;
        WaitingQueue.pTail = target;
    } else {
        WaitingQueue.pHead = target;
        WaitingQueue.pTail = target;
    }
    WaitingQueue.queueCount++;
    // printf("suspended!\n");
    return 0;
}

int thread_cancel(thread_t tid) {
    if (tid >= MAX_THREAD_NUM) {
        return -1;
    }

    Thread *target = pThreadTbEnt[tid].pThread;

    if (target == NULL || target == pCurrentThread) {
        return -1;
    }
    kill(target->pid, SIGKILL);
    Thread *prev_target = target->pPrev;
    Thread *next_target = target->pNext;

    if (target->status == THREAD_STATUS_READY) {
        if (ReadyQueue.pHead == target) {
            ReadyQueue.pHead = next_target;
        }
        if (ReadyQueue.pTail == target) {
            ReadyQueue.pTail = prev_target;
        }
        ReadyQueue.queueCount--;
    } else if (target->status == THREAD_STATUS_WAIT) {
        if (WaitingQueue.pHead == target) {
            WaitingQueue.pHead = next_target;
        }
        if (WaitingQueue.pTail == target) {
            WaitingQueue.pTail = prev_target;
        }
        WaitingQueue.queueCount--;
    }

    if (prev_target != NULL)
        prev_target->pNext = next_target;
    if (next_target != NULL)
        next_target->pPrev = prev_target;

    target->pNext = NULL;
    target->pPrev = NULL;
    pThreadTbEnt[tid].bUsed = 0;

    free(target->stackAddr);
    free(target);
    pThreadTbEnt[tid].pThread = NULL;

    return 0;
}

int thread_resume(thread_t tid) {
    if (tid >= MAX_THREAD_NUM) {
        return -1;
    }
    Thread *target = pThreadTbEnt[tid].pThread;

    if (target == NULL ||
        (target != NULL && target->status != THREAD_STATUS_WAIT)) {
        return -1;
    }

    Thread *prev_target = target->pPrev;
    Thread *next_target = target->pNext;

    if (WaitingQueue.pHead == target) {
        WaitingQueue.pHead = next_target;
    }
    if (WaitingQueue.pTail == target) {
        WaitingQueue.pTail = prev_target;
    }

    if (prev_target != NULL) {
        prev_target->pNext = next_target;
    }
    if (next_target != NULL) {
        next_target->pPrev = prev_target;
    }

    target->pPrev = NULL;
    target->pNext = NULL;
    WaitingQueue.queueCount--;

    target->status = THREAD_STATUS_READY;

    if (ReadyQueue.queueCount > 0) {
        target->pPrev = ReadyQueue.pTail;
        ReadyQueue.pTail->pNext = target;
        ReadyQueue.pTail = target;
    } else {
        ReadyQueue.pHead = target;
        ReadyQueue.pTail = target;
    }

    ReadyQueue.queueCount++;
    // printf("resumed!\n");
    return 0;
}

thread_t thread_self(void) {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pCurrentThread == pThreadTbEnt[i].pThread) {
            return i;
        }
    }
    return -1;
}

int thread_join(thread_t tid) {
    if (tid >= MAX_THREAD_NUM || tid < 0) {
        return -1;
    }
    Thread *target = pThreadTbEnt[tid].pThread;
    Thread *parent = pCurrentThread;
    Thread *newThread = ReadyQueue.pHead;
    int ppid = -1;
    int newpid = -1;
    if (parent == NULL || newThread == NULL) {
        return -1;
    }
    if (target == NULL ||
        (target != NULL && target->status == THREAD_STATUS_ZOMBIE)) {
        return 0;
    }
    ppid = parent->pid;
    newpid = newThread->pid;

    ReadyQueue.pHead = ReadyQueue.pHead->pNext;
    if (ReadyQueue.pHead != NULL) {
        ReadyQueue.pHead->pPrev = NULL;
    }

    newThread->pNext = NULL;
    newThread->status = THREAD_STATUS_RUN;
    pCurrentThread = newThread;
    ReadyQueue.queueCount--;

    parent->pNext = NULL;
    parent->pPrev = NULL;
    parent->status = THREAD_STATUS_WAIT;
    if (WaitingQueue.queueCount > 0) {
        parent->pPrev = WaitingQueue.pTail;
        WaitingQueue.pTail->pPrev = parent;
        WaitingQueue.pTail = parent;
    } else {
        WaitingQueue.pHead = parent;
        WaitingQueue.pTail = parent;
    }
    WaitingQueue.queueCount++;
    if (target->status != THREAD_STATUS_ZOMBIE) {
        __ContextSwitch(ppid, newpid);
    }
    if (target != NULL && target->status != THREAD_STATUS_ZOMBIE) {
        pause();
    }
    Thread *prevParent = parent->pPrev;
    Thread *nextParent = parent->pNext;

    if (WaitingQueue.pHead == parent) {
        WaitingQueue.pHead = nextParent;
    }
    if (WaitingQueue.pTail == parent) {
        WaitingQueue.pTail = prevParent;
    }

    if (prevParent != NULL) {
        prevParent->pNext = nextParent;
    }
    if (nextParent != NULL) {
        nextParent->pPrev = prevParent;
    }
    WaitingQueue.queueCount--;

    parent->status = THREAD_STATUS_READY;
    if (ReadyQueue.queueCount > 0) {
        parent->pPrev = ReadyQueue.pTail;
        ReadyQueue.pTail->pNext = parent;
        ReadyQueue.pTail = parent;
    } else {
        ReadyQueue.pHead = parent;
        ReadyQueue.pTail = parent;
        parent->pNext = NULL;
        parent->pPrev = NULL;
    }
    ReadyQueue.queueCount++;
    target->pPrev = NULL;
    target->pNext = NULL;
    free(target->stackAddr);
    free(target);
    pThreadTbEnt[tid].pThread = NULL;
    pThreadTbEnt[tid].bUsed = 0;
    return 0;
}
void thread_exit(void) {
    Thread *cur = pThreadTbEnt[thread_self()].pThread;
    if (cur == NULL) {
        return;
    }
    cur->status = THREAD_STATUS_ZOMBIE;
}
int thread_cputime(void) { return pCurrentThread->cpu_time; }
