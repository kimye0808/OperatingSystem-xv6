# OperatingSystem-xv6

이 저장소는 xv6 운영체제에서 구현한 세 가지 운영체제 프로젝트를 포함하고 있습니다.

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

---

## P2: Process Manager 및 Exec2, Set Memory Limit 구현

### 개요
이 프로젝트는 xv6 운영체제에서 프로세스 관리 기능을 확장하고, `exec` 시스템 콜을 개선하며, 프로세스에 메모리 한도를 설정하는 시스템 콜을 추가하는 작업을 수행합니다. 주요 구현은 **Process Manager**, **Exec2**, **Set Memory Limit**, **Light-weight Process (LWP)** 등에 중점을 두었습니다.

### 구현 세부사항

#### 1. Process Manager
- **목표**: 쉘처럼 사용자 입력을 받아 프로세스와 관련된 명령어를 처리합니다. 프로세스 종료, 메모리 제한 설정, 프로세스 실행 등을 제어합니다.
- **작동 방식**:
  1. 사용자로부터 입력을 받음
  2. 입력된 명령어를 분석하고 해당 작업 수행
  3. 입력에 맞는 기능을 수행하며 예외 처리
  4. 반복적으로 실행

- **주요 명령어**:
  - `kill`: 프로세스 종료
  - `memlim`: 프로세스의 메모리 제한 설정
  - `execute`: 새로운 프로세스 실행

#### 2. Exec2
- **목표**: `xv6`의 기존 `exec` 기능을 개선하여, 새로운 프로세스를 실행할 때 더 많은 스택 공간을 할당할 수 있도록 수정합니다.
- **변경 사항**: 기존의 `exec`는 1개의 스택 페이지를 할당했으나, `Exec2`는 사용자가 원하는 만큼 스택 페이지를 할당할 수 있습니다.

#### 3. Set Memory Limit
- **목표**: 프로세스가 사용할 수 있는 메모리의 최대 한도를 설정하여, `sbrk` 시스템 콜을 통해 프로세스가 메모리를 초과하여 할당하지 않도록 합니다.
- **작동 방식**: `growproc` 함수에서 메모리 할당 시, 설정된 메모리 제한을 초과하지 않도록 구현됩니다.

#### 4. Light-weight Process (LWP)
- **목표**: LWP는 여러 스레드가 동일한 페이지 테이블을 공유하는 방식으로, 프로세스와 유사한 독립적인 실행 단위를 구현합니다.
- **구현 방식**:
  - LWP는 메인 프로세스와 페이지 테이블을 공유하며, 새 스레드는 프로세스와 비슷한 방식으로 생성됩니다.
  - `fork`와 `exec` 과정에서 스레드를 프로세스로 변환하며, 스레드 간 메모리 공유를 위해 `pgdir`을 복사합니다.

### 주요 데이터 구조

```c
// Process Manager 관련 구조체
typedef struct _ProcManager {
    char *command;  // 명령어
    int status;     // 명령어 실행 상태
    // 기타 필요한 데이터들
} ProcManager;

// Exec2 관련 구조체
typedef struct _Exec2 {
    char *stack_pages;  // 스택 페이지들
    int num_pages;      // 할당된 스택 페이지 수
} Exec2;

// LWP 관련 구조체
typedef struct _LWP {
    struct proc *main_proc;   // 메인 프로세스
    struct proc *thread_proc; // 스레드 프로세스
    uint pgdir;               // 페이지 디렉토리
} LWP;
```

### 구현된 시스템 콜
1. `pmanager`: 프로세스 매니저 인터페이스 실행
2. `exec2`: 새로운 프로세스 실행 (확장된 스택 페이지 지원)
3. `setmemorylimit`: 특정 프로세스에 메모리 한도 설정
4. `createThread`: LWP 생성
5. `joinThread`: LWP 종료 대기

### 테스트
이 프로젝트는 다양한 시스템 콜과 프로세스 관리 기능을 테스트하기 위해 다음과 같은 파일들로 테스트되었습니다:
- `pmanager_test.c`: 프로세스 매니저 명령어 처리 테스트
- `exec2_test.c`: `exec2`의 정상 작동 여부 확인
- `lwp_test.c`: Light-weight Process 생성 및 종료 기능 테스트

---

## P3: [추후 추가 예정]