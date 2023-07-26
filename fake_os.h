#include "fake_process.h"
#include "linked_list.h"
#include <pthread.h>
#include <semaphore.h>
#pragma once

sem_t mutex;


typedef struct{
    ListItem list;
    int pid;
    ListHead events;
} FakePCB;

struct FakeOS;
typedef void (*ScheduleFn)(struct FakeOS *os, void *args, int id);

typedef struct FakeOS{
    FakePCB** running;
    ListHead ready;
    ListHead waiting;
    int timer;
    ScheduleFn schedule_fn;
    void *schedule_args;
    int cpus;
    ListHead processes;
} FakeOS;

typedef struct {
    FakeOS* os;
    int threadId;
} thread_args_t;

void scan_waiting_list(FakeOS* os);
void FakeOS_init(FakeOS *os, int cpus);
void* FakeOS_simStep(void *os);