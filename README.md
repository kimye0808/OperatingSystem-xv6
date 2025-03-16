# OperatingSystem-xv6

이 저장소는 xv6 운영체제에서 구현한 세 가지 운영체제 프로젝트를 포함하고 있습니다. 각 프로젝트는 운영체제 설계 및 구현의 다양한 측면에 초점을 맞추고 있습니다.

## P1: 다중 레벨 피드백 큐(MLFQ) 스케줄러 구현

### 개요
이 프로젝트는 xv6 운영체제에서 다중 레벨 피드백 큐(MLFQ) 스케줄러를 구현합니다. MLFQ는 세 개의 큐 레벨로 구성됩니다:
- L0 및 L1: 라운드 로빈(Round Robin) 스케줄링
- L2: 우선순위 기반 스케줄링, 동일 우선순위의 경우 선입선출(FCFS) 방식 사용

### 구현 세부사항

#### 큐 구조
- **L0 & L1**: 효율적인 라운드 로빈 스케줄링을 위해 원형 큐(circular queue)로 구현
- **L2**: 우선순위 스케줄링을 위해 최소 힙(min-heap) 기반 우선순위 큐로 구현

#### 글로벌 틱 시스템
- xv6의 내장 틱 대신 사용자 정의 글로벌 틱 카운터(`gTicks`) 구현
- 프로세스의 실행 시간(runtime)은 타이머 인터럽트 동안 실제로 CPU에서 실행될 때 증가
- 100 글로벌 틱마다 우선순위 부스팅(priority boosting) 발생

#### 큐 간 프로세스 이동
- 프로세스는 L0에서 시작
- L0에서 전체 타임 퀀텀(4 틱)을 사용하면 L1으로 이동
- L1에서 전체 타임 퀀텀을 사용하면 L2로 이동
- L2의 프로세스는 시간이 지남에 따라 우선순위가 감소
- 우선순위 부스팅은 모든 프로세스를 L0로 되돌림

#### 스케줄러 잠금/해제
- `schedulerLock(int password)`: 프로세스를 유일하게 스케줄링되도록 잠금
- `schedulerUnlock(int password)`: 이전에 잠긴 프로세스를 해제
- 보안을 위해 학번을 비밀번호로 사용
- 잘못된 사용에 대한 다양한 오류 처리

### 구현된 시스템 콜
1. `schedulerLock(int password)`
2. `schedulerUnlock(int password)`
3. `setPriority(int priority)`
4. `getLevel(void)`
5. `yield(void)`

### 주요 데이터 구조
```c
// 라운드 로빈용 원형 큐 (L0, L1)
typedef struct _Queue {
    int front;
    int rear;
    struct proc *p[QUEUE_MAX_SIZE];
} Queue;

// 최소 힙을 사용한 우선순위 큐 (L2)
typedef struct _pQueue {
    int count;
    struct proc *p[QUEUE_MAX_SIZE];
} pQueue;

// MLFQ 구조체
typedef struct _MLFQ {
    uint gTicks;
    Queue *L0;
    Queue *L1;
    pQueue *L2;
} Mlfq;
```

### 테스트
구현은 다음을 사용하여 테스트되었습니다:
- `mlfq_test.c`: 기본 MLFQ 기능 테스트
- `a_test.c`: 스케줄러 잠금/해제에 대한 엣지 케이스를 포함한 특정 스케줄러 동작 테스트

## P2: [추후 추가 예정]

## P3: [추후 추가 예정]