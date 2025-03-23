#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 1000000
#define NUM_PROCS 6

int main(void) {
    int i, j;
    int pid;
    int count[NUM_PROCS][3];
    int start_time[NUM_PROCS];
    int end_time[NUM_PROCS];
    int response_time[NUM_PROCS];
    int total_waiting_time = 0;

    // 초기화
    for (i = 0; i < NUM_PROCS; i++) {
        count[i][0] = count[i][1] = count[i][2] = 0;
        response_time[i] = -1;  // 초기값은 아직 응답하지 않은 상태를 의미
    }

    int global_start = uptime();

    for (i = 0; i < NUM_PROCS; i++) {
        if ((pid = fork()) == 0) { // 자식 프로세스 생성
            start_time[i] = uptime();
            for (j = 0; j < NUM_LOOP; j++) {
                int level = getLevel();
                count[i][level]++;

                if (response_time[i] == -1) {
                    response_time[i] = uptime() - start_time[i];
                }
            }
            end_time[i] = uptime();
            printf(1, "[Process %d] L0: %d, L1: %d, L2: %d\n", i, count[i][0], count[i][1], count[i][2]);
            printf(1, "[Process %d] Response Time: %d ticks\n", i, response_time[i]);
            printf(1, "[Process %d] Turnaround Time: %d ticks\n", i, end_time[i] - start_time[i]);
            exit();
        }
    }

    for (i = 0; i < NUM_PROCS; i++) {
        wait();
    }

    int global_end = uptime();

    // 전체 실행 시간
    int total_time = global_end - global_start;
    printf(1, "\nTotal Execution Time: %d ticks\n", total_time);

    exit();
}
