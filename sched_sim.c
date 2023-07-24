#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "fake_os.h"

FakeOS os;

typedef struct
{
    int quantum;
    int lambda;
} SchedRRArgs;

void schedRR(FakeOS *os, void *args_)
{
    SchedRRArgs *args = (SchedRRArgs *)args_;

    // look for the first process in ready
    // if none, return
    if (!os->ready.first)
        return;

    FakePCB *pcb = (FakePCB *)List_popFront(&os->ready);
    os->running = pcb;

    assert(pcb->events.first);
    ProcessEvent *e = (ProcessEvent *)pcb->events.first;
    assert(e->type == CPU);

    // look at the first event
    // if duration>quantum
    // push front in the list of event a CPU event of duration quantum
    // alter the duration of the old event subtracting quantum
    if (e->duration > args->quantum)
    {
        ProcessEvent *qe = (ProcessEvent *)malloc(sizeof(ProcessEvent));
        qe->list.prev = qe->list.next = 0;
        qe->type = CPU;
        qe->duration = args->quantum;
        e->duration -= args->quantum;
        List_pushFront(&pcb->events, (ListItem *)qe);
    }
};


void schedSJF(FakeOS *os, void *args_){
    FakePCB *pcb = (FakePCB *)List_popFront(&os->ready);
    os->running = pcb;

    

}

int main(int argc, char **argv)
{
    if (argc < 3){
        printf("usage: %s <num_cpus> <process1> <process2> ...\n", argv[0]);
        return -1;
    }
    if(!(cpus = atoi(argv[1]))){
        printf("invalid number of cpus\n");
        return -1;
    }
    FakeOS_init(&os);
    SchedRRArgs srr_args;
    srr_args.quantum = 5;
    srr_args.lambda = 0.3;
    os.schedule_args = &srr_args;
    os.schedule_fn = schedRR;


    for (int i = 2; i < argc; ++i)
    {
        FakeProcess new_process;
        int num_events = FakeProcess_load(&new_process, argv[i]);
        printf("loading [%s], pid: %d, events:%d\n",
               argv[i], new_process.pid, num_events);
        if (num_events != 0)
        {
            FakeProcess *new_process_ptr = (FakeProcess *)malloc(sizeof(FakeProcess));
            *new_process_ptr = new_process;
            List_pushBack(&os.processes, (ListItem *)new_process_ptr);
        }
    }
    printf("num processes in queue %d\n", os.processes.size);
    while (os.running || os.ready.first || os.waiting.first || os.processes.first)
    {
        FakeOS_simStep(&os);
        ++os.timer;
    }
}
