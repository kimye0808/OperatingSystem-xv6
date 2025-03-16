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

## P1: 다중 레벨 피드백 큐(MLFQ) 스케줄러 구현 - Troubleshooting

### 문제 1: 프로세스가 예상보다 오래 대기하는 문제
**원인**: 프로세스가 L2 큐에서 너무 오래 대기하는 경우가 있음. L2 큐는 우선순위가 낮은 프로세스를 처리하지만, 우선순위가 낮다고 해서 항상 짧은 시간 내에 실행되지는 않음. 특히, L2에서의 우선순위 부스팅이 제대로 작동하지 않거나, 우선순위 감소가 너무 빨리 일어나는 경우 문제가 발생할 수 있음.

**해결법**:
- 우선순위 부스팅 기능이 제대로 작동하는지 확인하고, 100틱마다 프로세스가 L0로 돌아가도록 설정했는지 점검.
- L2 큐에서 프로세스가 너무 오랫동안 대기하지 않도록, 우선순위 감소 속도를 조절하거나 추가적인 타임아웃을 설정.

### 문제 2: 시스템이 예상보다 느리게 작동하는 문제
**원인**: MLFQ 스케줄러에서 큐 간 이동 및 우선순위 변경 로직이 비효율적으로 작동하거나, 글로벌 틱 카운터 업데이트가 너무 자주 일어나면서 시스템 성능에 영향을 미칠 수 있음.

**해결법**:
- `gTicks` 카운터가 지나치게 자주 업데이트되지 않도록 설정하여 시스템 부하를 줄임.
- 프로세스 이동 및 우선순위 변경이 너무 복잡하게 구현되어 있다면, 큐 간 프로세스 이동을 더 간단하고 효율적인 방식으로 리팩토링.

### 문제 3: 잘못된 스케줄러 잠금/해제 동작
**원인**: `schedulerLock` 및 `schedulerUnlock` 함수에서 비밀번호가 정확하지 않거나, 잠금/해제 과정에서 동기화 문제가 발생할 수 있음.

**해결법**:
- `schedulerLock` 및 `schedulerUnlock`에 대한 오류 처리 로직을 강화하고, 비밀번호가 정확히 입력되었는지 확인.
- 잠금/해제 과정에서 발생할 수 있는 경쟁 조건을 방지하기 위해 `schedulerLock`과 `schedulerUnlock` 함수의 동작을 더 명확하게 정의하고, 동기화 문제를 해결.

### 문제 4: 프로세스가 예기치 않게 종료되는 문제
**원인**: 프로세스가 L0에서 L1, L2로 이동하면서 예기치 않게 종료될 수 있음. 특히, 프로세스가 종료되지 않았는데도 `exit` 시스템 콜이 호출될 경우 문제가 발생할 수 있음.

**해결법**:
- 프로세스가 큐 간에 이동할 때마다 해당 프로세스의 상태를 명확하게 확인하고, `exit` 시스템 콜이 호출되는 조건을 엄격히 정의.
- 프로세스 상태가 예기치 않게 변경되지 않도록 스케줄러 동작을 점검하고, `schedulerLock`과 `schedulerUnlock`이 올바르게 적용되었는지 확인.

--- 

## P2: Process Manager 및 LWP 구현

### 개요
이 프로젝트는 xv6 운영체제에서 프로세스 관리 기능을 확장하고, 확장된 스택 페이지를 지원하는 exec2 시스템 콜과 프로세스 메모리 한도를 설정하는 기능을 구현합니다. 또한 Light-weight Process(LWP)를 통해 스레드 기능을 지원합니다.

### 구현 세부사항

#### Process Manager
- 사용자 입력을 받아 프로세스와 관련된 명령어를 처리하는 인터페이스 제공
- 프로세스 종료, 메모리 제한 설정, 프로세스 실행 등을 제어
- 명령어 분석 및 해당 작업 수행, 예외 처리

#### Exec2 시스템 콜
- 기존 `exec` 시스템 콜을 개선하여 더 많은 스택 페이지 할당 지원
- 사용자가 원하는 만큼의 스택 페이지를 할당할 수 있도록 수정

#### Set Memory Limit
- 프로세스가 사용할 수 있는 메모리의 최대 한도 설정
- `sbrk` 시스템 콜을 통해 프로세스가 메모리를 초과 할당하지 않도록 제한
- `growproc` 함수에서 메모리 할당 시 설정된 한도 체크

#### Light-weight Process (LWP)
- 여러 스레드가 동일한 페이지 테이블을 공유하는 프로세스와 유사한 실행 단위
- 메인 프로세스와 페이지 테이블 공유, 새 스레드는 프로세스와 유사하게 생성
- `fork`와 `exec` 과정에서 스레드를 프로세스로 변환하고 메모리 공유

### 구현된 시스템 콜
1. `pmanager`: 프로세스 매니저 인터페이스 실행
2. `exec2`: 새로운 프로세스 실행 (확장된 스택 페이지 지원)
3. `setmemorylimit`: 특정 프로세스에 메모리 한도 설정
4. `createThread`: LWP 생성
5. `joinThread`: LWP 종료 대기

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

### 테스트
구현은 다음을 사용하여 테스트되었습니다:
- `pmanager_test.c`: 프로세스 매니저 명령어 처리 테스트
- `exec2_test.c`: `exec2`의 정상 작동 여부 확인
- `lwp_test.c`: Light-weight Process 생성 및 종료 기능 테스트

## P2: Process Manager 및 LWP 구현 - Troubleshooting

### 문제 1: 스레드 생성 및 관리 방식의 혼동
**원인**: 스레드를 프로세스로 취급하기 위해 `proc` 구조체를 그대로 활용하는 설계에서, 스레드와 프로세스 간의 구분이 모호해져 관리가 어려운 상황이 발생할 수 있음. 또한, 스레드를 위한 멤버 변수를 `proc` 구조체에 추가하는 방식이 나중에 성능 저하를 초래할 가능성도 있음.

**해결법**:
- 스레드와 프로세스를 구분할 수 있도록 `proc` 구조체에 `thread` 배열을 추가하여 여러 스레드를 관리할 수 있도록 설계.
- `proc` 구조체 내에서 스레드를 구분할 수 있는 명확한 방법을 추가하여 각 스레드가 독립적인 실행 단위로 관리될 수 있도록 함.
- 성능을 고려하여 스레드와 프로세스 관리 방식을 적절히 타협.

### 문제 2: 스레드 스택 페이지 할당 및 해제 관련 오류
**원인**: 스레드의 스택 페이지 할당 및 해제 과정에서 문제가 발생, 특히 `fork` 시스템 콜에서 `copyuvm`을 사용하여 스택을 복사할 때 할당된 메모리 영역의 해제 문제가 발생. 스택 페이지가 제대로 할당되지 않거나 할당된 페이지가 올바르게 해제되지 않아 메모리 관리에 문제가 발생할 수 있음.

**해결법**:
- `sz` 값으로 스택 크기를 관리하며, 할당된 스택 페이지를 별도의 배열에 저장하여 해제 시 문제를 해결.
- `proc` 구조체에 `spare` 배열을 추가하여 해제된 메모리 주소를 추적하고, `thread_join`에서 이를 적절히 처리하도록 수정.
- `vm.c`에 정의된 메모리 할당 및 복사 해제 함수들을 추가하여 스택 할당과 해제가 문제 없이 이루어지도록 함.

### 문제 3: 스레드와 프로세스 메모리 공유 문제
**원인**: 스레드가 메인 프로세스와 페이지 테이블을 공유하므로, `fork` 및 `exec` 과정에서 메모리 주소 공간이 제대로 분리되지 않아 스레드 간 충돌이 발생할 수 있음.

**해결법**:
- `fork`와 `exec` 시스템 콜에서 스레드가 메인 프로세스와 적절히 메모리를 공유하면서 독립적으로 실행될 수 있도록 설계 변경.
- 스레드가 독립적인 메모리 공간을 가질 수 있도록 적절히 `page directory`를 설정하고, 공유되는 부분과 독립적인 부분을 명확히 구분.

--- 

## P3: Multi Indirect, Symbolic Link, Buffered I/O 구현

### 개요
이 프로젝트는 xv6 운영체제의 파일 시스템을 확장하여, 더 큰 파일을 저장할 수 있는 Multi Indirect 주소 방식을 구현하고, Symbolic Link 기능을 추가하며, 성능 향상을 위한 Sync 기능을 구현합니다.

### 구현 세부사항

#### Multi Indirect
- 기존 Xv6의 Direct와 Single indirect 방식을 확장하여 Double Indirect와 Triple Indirect 주소 공간 추가
- 더 많은 데이터를 한 파일에 저장할 수 있도록 개선
- `bmap` 함수를 수정하여 Multi Indirect 주소 처리 방식 구현

#### Symbolic Link
- Hard Link 외에 Symbolic Link 기능 지원
- `sys_link`와 유사한 방식으로 Symbolic Link 생성
- `create`, `readi`, `writei` 등을 활용하여 링크 처리
- 링크 삭제 시 `namei`를 통해 경로 정상 처리

#### Sync
- 버퍼된 I/O 방식을 적용하여 성능 향상
- `sync` 함수 호출 시에만 flush를 수행하도록 변경
- log commit 상태 체크 및 필요 시 commit 수행

### 구현된 시스템 콜
1. `symLink`: Symbolic Link 생성
2. `sync`: 버퍼된 I/O 동기화

### 주요 데이터 구조 및 매크로
```c
#define FSSIZE       2100000  // 파일 시스템 크기 (블록 단위)
#define NDIRECT (12-2)  // d_indir, t_indir 공간
#define D_NINDIRECT_ADRS  11
#define T_NINDIRECT_ADRS 12
#define NINDIRECT (BSIZE / sizeof(uint))  // 일반 indirect
#define D_NINDIRECT ((NINDIRECT) * (NINDIRECT))  // double indirect
#define T_NINDIRECT ((D_NINDIRECT) * (NINDIRECT))  // triple indirect
#define MAXFILE ((NDIRECT) + (NINDIRECT) + (D_NINDIRECT) + (T_NINDIRECT))  // 최대 파일 크기

// 수정된 inode 구조체
struct inode {
    // ...
    uint addrs[NDIRECT+1+2];   // 데이터 블록 주소 / multi indirect용
};
```

### 테스트
구현은 다음을 사용하여 테스트되었습니다:
- `stressfs`: 파일 시스템 스트레스 테스트
- `ls` 명령어: 파일 목록 정상 출력 확인
- Symbolic Link와 Multi Indirect 기능 테스트를 위한 특수 테스트 케이스

---

## P3: Symbolic Link 구현 및 관련 Troubleshooting

### 문제 1: Multi-indirect block 처리에서의 혼동
**원인**: `bmap` 함수에서 block number를 구할 때, multi-indirect block을 처리하는 부분에서 혼동이 생겨서 제대로 block 번호를 계산하지 못했음. 이로 인해 많은 시간이 걸림.

**해결법**: 
- `bmap`에서 multi-indirect block을 처리할 때, block 번호 계산 과정에 신경을 더 썼어야 했고, block을 다루는 방식을 더 명확히 이해하고 처리했어야 했음.
- 코드를 단계별로 다시 점검하며 정확하게 계산되는지 확인하고, 중간 결과를 체크해보며 디버깅을 진행.

### 문제 2: `itrunc` 함수에서의 삼중 루프 복잡성
**원인**: `itrunc` 함수에서 삼중 루프를 작성하면서 block을 read하고 free하는 과정에서 여러 변수를 다루다 보니 혼동이 생겨 시간이 지체됐음.

**해결법**:
- 여러 변수를 한 번에 다루는 대신, 필요한 부분을 더 나누어 구조를 단순화하고, block을 처리하는 과정에서 어떤 단계에서 문제가 발생하는지 세부적으로 확인했어야 했음.
- 메모리 할당과 해제 과정에서 변수나 block 상태를 정확히 추적하며 진행했어야 함.

### 문제 3: Symbolic Link 구현 어려움
**원인**: 처음에는 `link` 함수와 `dirlink`의 내용을 가져와 수정해서 하려고 했는데, 이 방식이 복잡하고 헷갈려서 구현이 힘들었음.

**해결법**:
- `OS 15 이론 pdf`에서 `copy = create + read + write`를 보고, `create` 함수를 사용해볼 수 있을 것 같아서 `create`를 활용하여 구현을 했음.
- 그 후에 symbolic link의 구현을 좀 더 명확히 이해하고, 필요한 부분만 수정하여 완성할 수 있었음.