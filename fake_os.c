#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "fake_os.h"

void FakeOS_init(FakeOS *os, int cpus){
    List_init(&os->ready);
    List_init(&os->waiting);
    List_init(&os->processes);
    os->timer = 0;
    os->schedule_fn = 0;
    os->cpus = cpus;
    os->running = (FakePCB**) malloc(cpus * sizeof(FakePCB*));
    for (int i = 0; i < cpus; ++i){
        os->running[i] = 0; //(FakePCB*)malloc(sizeof(FakePCB));
    }
}

void FakeOS_createProcess(FakeOS *os, FakeProcess *p, int id){
    // sanity check
    assert(p->arrival_time == os->timer && "time mismatch in creation");
    // we check that in the list of PCBs there is no
    // pcb having the same pid
    assert((!os->running[id] || os->running[id]->pid != p->pid) && "pid taken");

    ListItem *aux = os->ready.first;
    while (aux){
        FakePCB *pcb = (FakePCB *)aux;
        assert(pcb->pid != p->pid && "pid taken");
        aux = aux->next;
    }

    aux = os->waiting.first;
    while (aux){
        FakePCB *pcb = (FakePCB *)aux;
        assert(pcb->pid != p->pid && "pid taken");
        aux = aux->next;
    }

    // all fine, no such pcb exists
    FakePCB *new_pcb = (FakePCB *)malloc(sizeof(FakePCB));
    new_pcb->list.next = new_pcb->list.prev = 0;
    new_pcb->pid = p->pid;
    new_pcb->events = p->events;

    assert(new_pcb->events.first && "process without events");

    // depending on the type of the first event
    // we put the process either in ready or in waiting
    ProcessEvent *e = (ProcessEvent *)new_pcb->events.first;
    switch (e->type){
    case CPU:
        List_pushBack(&os->ready, (ListItem *)new_pcb);
        break;
    case IO:
        List_pushBack(&os->waiting, (ListItem *)new_pcb);
        break;
    default:
        assert(0 && "illegal resource");
        ;
    }
}

void* FakeOS_simStep(void *arg_os){
    thread_args_t* args = (thread_args_t*)arg_os;
    FakeOS* os = args->os;
    int id = args->threadId;
    int ret;

    printf("[CPU %d] Thread with id: %d just started\n", id, id);

    // scan process waiting to be started
    // and create all processes starting now
    ListItem *aux = os->processes.first;

    ret = sem_wait(&mutex);
    if(ret)perror("\033[1;31mSem_wait failed\n");

    while (aux){
        FakeProcess *proc = (FakeProcess *)aux;
        FakeProcess *new_process = 0;
        if (proc->arrival_time == os->timer){
            new_process = proc;
        }
        aux = aux->next;
        if (new_process){
            printf("\t[CPU %d] create pid:%d\n",id , new_process->pid);
            new_process = (FakeProcess *)List_detach(&os->processes, (ListItem *)new_process);
            FakeOS_createProcess(os, new_process, id);
            free(new_process);
        }
    }
    printf("\t[CPU %d] ready list size: %d\n", id, os->ready.size);
    printf("\t[CPU %d] waiting list size: %d\n", id, os->waiting.size);

    // scan waiting list, and put in ready all items whose event terminates
    aux = os->waiting.first;
    while (aux) {
        FakePCB *pcb = (FakePCB *)aux;
        aux = aux->next;
        ProcessEvent *e = (ProcessEvent *)pcb->events.first;
        printf("\t[CPU %d] waiting pid: %d\n", id, pcb->pid);
        assert(e->type == IO);
        e->duration--;
        printf("\t\t[CPU %d] remaining time:%d\n", id, e->duration);
        if (e->duration == 0) {
            printf("\t\t[CPU %d] end burst\n", id);
            List_popFront(&pcb->events);
            free(e);
            List_detach(&os->waiting, (ListItem *)pcb);
            if (!pcb->events.first) {
                // kill process
                printf("\t\t[CPU %d] end process\n", id);
                free(pcb);
            }else{
                // handle next event
                e = (ProcessEvent *)pcb->events.first;
                switch (e->type)
                {
                case CPU:
                    printf("\t\t[CPU %d] move to ready\n", id);
                    List_pushBack(&os->ready, (ListItem *)pcb);
                    break;
                case IO:
                    printf("\t\t[CPU %d] move to waiting\n", id);
                    List_pushBack(&os->waiting, (ListItem *)pcb);
                    break;
                }
            }
        }
    }

    // decrement the duration of running
    // if event over, destroy event
    // and reschedule process
    // if last event, destroy running
    FakePCB *running = os->running[id];
    printf("\t[CPU %d] running pid: %d\n", id, running ? running->pid : -1);
    if (running){
        ProcessEvent *e = (ProcessEvent *)running->events.first;
        assert(e->type == CPU);
        e->duration--;
        printf("\t\t[CPU %d] remaining time:%d\n", id, e->duration);
        if (e->duration == 0){
            printf("\t\t[CPU %d] end burst\n", id);
            List_popFront(&running->events);
            free(e);
            if (!running->events.first){
                printf("\t\t[CPU %d] end process\n", id);
                free(running); // kill process
            }else{
                e = (ProcessEvent *)running->events.first;
                switch (e->type){
                case CPU:
                    printf("\t\t[CPU %d] move to ready\n", id);
                    List_pushBack(&os->ready, (ListItem *)running);
                    break;
                case IO:
                    printf("\t\t[CPU %d] move to waiting\n", id);
                    List_pushBack(&os->waiting, (ListItem *)running);
                    break;
                }
            }
            os->running[id] = 0;
        }
    }

    // call schedule, if defined
    if (os->schedule_fn && !os->running[id]){
        (*os->schedule_fn)(os, os->schedule_args, id);
        printf("\t[CPU %d] scheduler pushed pid %d in running\n", id,  os->running[id] ? os->running[id]->pid : -1);
    }

    // if running not defined and ready queue not empty
    // put the first in ready to run
    if (!os->running[id] && os->ready.first){
        os->running[id] = (FakePCB *)List_popFront(&os->ready);
    }

    sem_post(&mutex);
    if(ret)perror("\033[1;31mSem_post failed\n");

    free(arg_os);

    printf("[CPU %d] Thread %d finished\n", id, id);

    return NULL;
}

