#define _GNU_SOURCE
#include "scheduler.h"
#include "init.h"
#include "thread.h"
#include <stdio.h>
#include <time.h>
void __ContextSwitch(int curpid, int newpid) {
    Thread *cur = NULL;
    Thread *new = NULL;
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].pThread != NULL &&
            pThreadTbEnt[i].pThread->pid == curpid) {
            cur = pThreadTbEnt[i].pThread;
            break;
        }
    }
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (pThreadTbEnt[i].pThread != NULL &&
            pThreadTbEnt[i].pThread->pid == newpid) {
            new = pThreadTbEnt[i].pThread;
            break;
        }
    }
    if (new != NULL) {
        new->cpu_time += TIMESLICE;
    }
    if (cur != NULL && cur->status == THREAD_STATUS_WAIT && newpid > 0) {
        kill(newpid, SIGCONT);
        pause();
    } else if (curpid < 0 && newpid >= 0) {
        kill(newpid, SIGCONT);
    } else if (curpid > 0 && newpid <= 0) {
        kill(curpid, SIGSTOP);
    } else if (curpid > 0 && newpid > 0) {
        kill(curpid, SIGSTOP);
        kill(newpid, SIGCONT);
    } else {
        return;
    }
}

void RunScheduler(void) {

    timer_t timerid;
    timer_create(CLOCK_REALTIME, NULL, &timerid);
    struct itimerspec setTime;
    setTime.it_interval.tv_sec = TIMESLICE;
    setTime.it_interval.tv_nsec = 0;
    setTime.it_value.tv_sec = TIMESLICE;
    setTime.it_value.tv_nsec = 0;
    timer_settime(timerid, 0, &setTime, NULL);
}
