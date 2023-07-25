#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fake_os.h"

FakeOS os;

typedef struct{
    int quantum;
    float lambda;
} SchedSJFargs;

FakePCB* findSJ(FakeOS *os){
    int n_proc = os->ready.size;
    FakePCB* shortest = NULL;
    FakePCB* tmp = (FakePCB*)os->ready.first;
    int lowest = 0;
    for (int i = 0; i < n_proc; ++i){
        ProcessEvent* pe = (ProcessEvent*)tmp->events.first;
        if(pe->type == CPU){
            int prediction = pe->prediction;
            if(i == 0) {
                lowest = prediction;
                shortest = tmp;
            }
            if(prediction < lowest){
                shortest = tmp;
                lowest = prediction;
            }
        }
        tmp = (FakePCB*)tmp->list.next;
    }
    return shortest;
}

void schedSJF(FakeOS *os, void *args_){
    SchedSJFargs *args = (SchedSJFargs *)args_;

    // look for the first process in ready
    // if none, return
    if (!os->ready.first)
        return;

    printf("Firsts events for processes in ready\n");
    FakePCB* tmp = (FakePCB*) os->ready.first;
    ProcessEvent* temp = (ProcessEvent*) tmp->events.first;
    for (int i = 0; i < os->ready.size; ++i){
        if (temp->type == CPU){
            printf("\t-------Pid: %d-------\n", tmp->pid);
            printf("\tType: %s\n", temp->type==0?"CPU":"IO");
            printf("\tDuration: %d\n", temp->duration);
            printf("\tPrediction: %f\n", temp->prediction);
        }
        if(tmp->list.next && tmp->events.first){
            tmp = (FakePCB*) tmp->list.next;
            temp = (ProcessEvent*)tmp->events.first;
        }
    }

    FakePCB *pcb = (FakePCB*) findSJ(os);
    List_detach(&os->ready, (ListItem*)pcb);

    os->running = pcb;

    assert(pcb->events.first);
    ProcessEvent* e = (ProcessEvent*)pcb->events.first;
    assert(e->type == CPU);

    printf("Shortest found pid: %d, prediction: %f\n", pcb->pid, e->prediction);

    if (e->duration > args->quantum){
        ProcessEvent *qe = (ProcessEvent *)malloc(sizeof(ProcessEvent));
        qe->list.prev = qe->list.next = 0;
        qe->type = CPU;
        qe->duration = args->quantum;
        e->duration -= args->quantum;
        e->prediction = (e->prediction * (1-args->lambda)) + (e->duration * args->lambda);
        qe->prediction = e->prediction;
        List_pushFront(&pcb->events, (ListItem *)qe);
    }
}



int main(int argc, char **argv){
    if (argc < 3){
        printf("\033[1;31mUSAGE: %s <num_cpus> <process1> <process2> ...\n", argv[0]);
        return -1;
    }
    int cpus;
    if(!(cpus = atoi(argv[1]))){
        printf("\033[1;31mInvalid number of cpus\n");
        return -1;
    }
    FakeOS_init(&os);
    SchedSJFargs sjf_args;
    sjf_args.quantum = 5;
    sjf_args.lambda = 0.8;
    os.schedule_args = &sjf_args;
    os.schedule_fn = schedSJF;
    os.cpus = cpus;


    for (int i = 2; i < argc; ++i){
        FakeProcess new_process;
        int num_events = FakeProcess_load(&new_process, argv[i]);
        printf("loading [%s], pid: %d, events:%d\n",
               argv[i], new_process.pid, num_events);
        if (num_events != 0){
            FakeProcess *new_process_ptr = (FakeProcess *)malloc(sizeof(FakeProcess));
            *new_process_ptr = new_process;
            List_pushBack(&os.processes, (ListItem *)new_process_ptr);
        }
    }
    printf("num processes in queue %d\n", os.processes.size);
    while (os.running || os.ready.first || os.waiting.first || os.processes.first){
        int ret;
        pthread_t* tids = (pthread_t*)malloc(cpus * sizeof(pthread_t));
        for (int i = 0; i < cpus; ++i){
            pthread_t tid = tids[i];
            ret = pthread_create(&tid, NULL, FakeOS_simStep, (void*)&os);
            if(ret) perror("\033[1;31mCreation of the thread failed\n");
        }
        for (int i = 0; i < cpus; ++i){
            ret = pthread_join(tids[i], NULL);
            if(ret) perror("\033[1;31mJoin of the thread failed\n");
        }
        //FakeOS_simStep(&os);
        ++os.timer;
        free(tids);
    }
}
