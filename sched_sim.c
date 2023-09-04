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

void schedSJF(FakeOS *os, void *args_, int id){
    SchedSJFargs *args = (SchedSJFargs *)args_;

    // look for the first process in ready
    // if none, return
    if (!os->ready.first)
        return;

    //*********For debug only *********
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
    //********************************

    FakePCB *pcb = (FakePCB*) findSJ(os);
    List_detach(&os->ready, (ListItem*)pcb);

    os->running[id] = pcb;

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
    FakeOS_init(&os, cpus);
    SchedSJFargs sjf_args;
    sjf_args.quantum = 5;
    sjf_args.lambda = 0.8;
    os.schedule_args = &sjf_args;
    os.schedule_fn = schedSJF;


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

    int ret;
    ret = sem_init(&mutex, 0, 1);
    if(ret)perror("\033[1;31mSem_init failed\n");

    while (1){
        int escape = 0;
        for (int i = 0; i < cpus+1; i++){
            if(i < cpus){
                if(!os.running[i]) escape++;
            }else{
                if(!os.ready.first) escape++;
                if(!os.waiting.first) escape++;
                if(!os.processes.first) escape++;
            }
        }
        if (escape == cpus + 3)break;

        printf("************** TIME: %08d **************\n", os.timer);

        scan_waiting_list(&os);

        //pthread_t* tids = (pthread_t*)malloc(cpus * sizeof(pthread_t));
        pthread_t tids[cpus];

        for (int i = 0; i < cpus; ++i){
            thread_args_t* args = (thread_args_t*) malloc(sizeof(thread_args_t));
            args->os = &os;
            args->threadId = i;
            ret = pthread_create(&tids[i], NULL, FakeOS_simStep, (void*)args);
            if(ret) perror("\033[1;31mCreation of the thread failed\n");
        }

        for (int i = 0; i < cpus; ++i){
            ret = pthread_join(tids[i], NULL);
            if(ret) perror("\033[1;31mJoin of the thread failed\n");
        }
        
        printf("All the threads were joined\n");

        ++os.timer;
    }
    ret = sem_destroy(&mutex);
    if(ret)perror("\033[1;31mSem_destroy failed\n");
}
