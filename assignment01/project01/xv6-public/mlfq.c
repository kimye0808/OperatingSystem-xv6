#include "mlfq.h"

struct spinlock mlfqLock;

//check circular queue empty return 1 == empty
int queue_isEmpty(Queue *q)
{
    return (q->front == q->rear);
}

int queue_isFull(Queue *q)
{
    return (q->rear+1 % QUEUE_MAX_SIZE) == q->front;
}
void enQueue(Queue *q, struct proc *ps)
{
    if(queue_isFull(q)){
        cprintf("queue is full\n");
    }
    q->rear = ((q -> rear)+1) % QUEUE_MAX_SIZE; 
    q->p[q->rear] = ps;
}

struct proc* deQueue(Queue *q){
    if(queue_isEmpty(q)){
        cprintf("queue empty error\n");
    } 
    q->front = ((q->front)+1)%QUEUE_MAX_SIZE;
    struct proc *result = q->p[q->front];
    //q->p[q->front] = 0; //dequeue then replace it 0
    //cprintf("%d dequeue %d ql: %d\n",mlfq.gTicks, result->pid, result->qLevel);

    return result;
}

int queue_isThereRunnable(Queue *q)
{
    struct proc *pr;
    int flag = 0;
    if(queue_isEmpty(q)) return 0;
    int A_size = ((q->rear - q->front) + QUEUE_MAX_SIZE) % QUEUE_MAX_SIZE;
    for(int idx = 0; idx < A_size; idx++)
    {
        pr = deQueue(q);
        if(pr->state == RUNNABLE){
            flag = 1;
        }
        enQueue(q, pr);
    }
    return flag;
}
//move all process from Queue A to Queue B for priority boosting
void queue_moveAllProcAtoB(Queue *A, Queue *B)
{
    struct proc *pr;
    if(queue_isEmpty(A)) return;
    int A_size = ((A->rear - A->front) + QUEUE_MAX_SIZE) % QUEUE_MAX_SIZE;
    for(int idx = 0; idx < A_size; idx++)
    {
        pr = deQueue(A);
        pr->qLevel = 0;
        pr->priority = 3;
        pr->runTime = 0;
        pr->l2Time = 0;
        enQueue(B, pr);
    }
}
//find one process in A if A is pr, reset queue level priority and move to B ,or A again
void queue_findOneProcThenMoveAtoB(Queue *q, struct proc *pr)
{
    struct proc *tmp = 0;
    int A_size = ((q->rear - q->front) + QUEUE_MAX_SIZE) % QUEUE_MAX_SIZE;
    for(int idx = 0; idx < A_size; idx++)
    {
        tmp = deQueue(q);
        if(tmp == pr)
        {
            tmp->qLevel = 0;
            tmp->priority = 3;
            tmp->runTime = 0;
            tmp->l2Time = 0;
            enQueue(mlfq.L0, tmp);   
            continue;
        }
        enQueue(q, tmp);
    }
}
//find one process in A if A is pr, move to B ,or A again,,,,,,pr is dequeue state
void queue_timeQuantumOverflow_toQ(struct proc *pr)
{
    pr->qLevel = 1;
    pr->runTime = 0;
    enQueue(mlfq.L1, pr);
    //cprintf("L0 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! pid : %d L0->L1\n", pr->pid);
}
//find one process in A if A is pr, move to B ,or A again move to L2
void queue_timeQuantumOverflow_toPQ(struct proc *pr)
{
    pr->qLevel = 2;
    pr->runTime = 0;
    pr->l2Time = mlfq.gTicks;
    pq_enQueue(mlfq.L2, pr);
    //cprintf("L1 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!overflow->L2\n");
}

///////////////////////////////////////priority queue//////////////////////////////////////////////
struct proc* peekPQ(pQueue *pq){
    return pq->p[1];
}

void pq_enQueue(pQueue *pq, struct proc *ps){
    if(pq->count == QUEUE_MAX_SIZE){
        cprintf("pq Overflow\n");
        return;
    }
    int i = ++(pq->count);
    pq->p[i] = ps;
    if(ps->state == RUNNABLE){
        while(i>1 && pq->p[i/2] && pq->p[i]){
            if(pq->p[i/2]->priority > (pq->p[i])->priority){
                struct proc *tmp = pq->p[i];
                pq->p[i] = pq->p[i/2];
                pq->p[i/2] = tmp;
            }else if(pq->p[i/2]->priority == (pq->p[i])->priority){
                if(pq->p[i/2]->l2Time > (pq->p[i])->l2Time){
                    struct proc *tmp = pq->p[i];
                    pq->p[i] = pq->p[i/2];
                    pq->p[i/2] = tmp;
                }
            }
            i = i/2;
        }
    }

}

void pq_min_heapify(pQueue *pq, int i){
    int min = i;
    int left = 2*i;
    int right = 2*i+1;
    if(left<=pq->count && pq->p[left] && pq->p[i] && pq->p[left]->priority < pq->p[i]->priority){
        if(pq->p[left]->priority == pq->p[i]->priority){
            if(pq->p[left]->l2Time < pq->p[i]->l2Time){
                min = left;
            }
        }
        min = left;
    }
    if(right <= pq->count && pq->p[left] && pq->p[i] && pq->p[right]->priority < pq->p[min]->priority){
        if(pq->p[right]->priority == pq->p[i]->priority){
            if(pq->p[right]->l2Time < pq->p[i]->l2Time){
                min = right;
            }
        }
        min = right;
    }
  // 자식 노드의 값이 더 작다면 교환하고 교환된 자식 노드부터 heapify 진행
    if(min != i){
        struct proc *tmp = pq->p[i];
        pq->p[i] = pq->p[min];
        pq->p[min] = tmp;
        pq_min_heapify(pq, min);
    }
}

struct proc* pq_deQueue(pQueue *pq){
    if(pq->count == 0){
        cprintf("pq is empty! cannot deq");
    }
  // 루트 노드 값을 리턴값에 저장한 뒤, 마지막 요소를 루트노드에 배치함
    struct proc *min = pq->p[1];
    pq->p[1] = pq->p[pq->count];
    pq->p[pq->count] = 0;
    pq->count--;

  // 루트 노드부터 heapify 수행
    pq_min_heapify(pq, 1);

    return min;
}

//move all process from pQueue A to Queue B for priority boosting
void pq_moveAllProcAtoQueueB(pQueue *A, Queue *B, int pid)
{
    struct proc *pr;
    int A_size = A->count;
    for(int idx = 0; idx < A_size;idx++)
    {
        pr = pq_deQueue(A);
        if(pr->pid == pid) continue;
        pr->qLevel = 0;
        pr->priority = 3;
        pr->runTime = 0;
        pr->l2Time = 0;
        enQueue(B, pr);
    }
}
//find one process in A if A is pr,  reset queue level priority and move to queue B ,or A again
void pq_findOneProcThenMoveAtoQueueB(pQueue *A, Queue *B, struct proc *pr)
{
    struct proc *tmp;
    int A_size = A->count;
    for(int idx = 0; idx < A_size;idx++)
    {
        tmp = pq_deQueue(A);
        if(tmp == pr)
        {
            tmp->qLevel = 0;
            tmp->priority = 3;
            tmp->runTime = 0;
            tmp->l2Time = 0;
            enQueue(B, tmp);
            continue;
        }
        pq_enQueue(A, tmp);
    }
}
//find one process in A if A is pr, set priority then move to A again
void pq_timeQuantumOverflow(struct proc *pr)
{
    //cprintf("p2!!!!!!!!!!!!!!!!!overFLOW %d pid %d\n", pr->priority, pr->pid);
    if(pr->priority > 0){
        pr->priority--;
    }           
    pr->runTime = 0;
    //do not need to en pq again in L2 because no where from L2, scheduler do it
}


