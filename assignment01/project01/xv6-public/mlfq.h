#pragma once

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

//circular queue for RR
typedef struct _Queue{
    int front;
    int rear;
    struct proc *p[QUEUE_MAX_SIZE];
}Queue;
//prioiry queue using min heap
typedef struct _pQueue{
    int count;
    struct proc *p[QUEUE_MAX_SIZE];
}pQueue;

typedef struct _MLFQ{
    uint gTicks;
    Queue *L0;
    Queue *L1;
    pQueue *L2;
}Mlfq;

//L0 queue
extern Queue q0;
//L1 queue
extern Queue q1;
//L2 queue
extern pQueue q2;
//mlfq
extern Mlfq mlfq;

//extern struct spinlock mlfqLock;

int queue_isThereRunnable(Queue *q);
int queue_isEmpty(Queue *q);
int queue_isFull(Queue *q);
void enQueue(Queue *q, struct proc *ps);
struct proc* deQueue(Queue *q);
struct proc* peekPQ(pQueue *pq);
void queue_moveAllProcAtoB(Queue *A, Queue *B);
void queue_findOneProcThenMoveAtoB(Queue *q, struct proc *pr);
void queue_timeQuantumOverflow_toQ(struct proc *pr);
void queue_timeQuantumOverflow_toPQ(struct proc *pr);

void pq_enQueue(pQueue *pq, struct proc *ps);
void pq_min_heapify(pQueue *pq, int i);
struct proc* pq_deQueue(pQueue *pq);
void pq_moveAllProcAtoQueueB(pQueue *A, Queue *B, int pid);
void pq_findOneProcThenMoveAtoQueueB(pQueue *A, Queue *B, struct proc *pr);
void pq_timeQuantumOverflow(struct proc *pr);


