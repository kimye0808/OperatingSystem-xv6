#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_LOOP 1000000
#define NUM_PROCS 6

int main(void) {
    int i, j;
    int pid;
    int start_time[NUM_PROCS];
    int first_run_time[NUM_PROCS];
    int end_time[NUM_PROCS];
    int response_time[NUM_PROCS];
    int turnaround_time[NUM_PROCS];

    // 초기화
    for (i = 0; i < NUM_PROCS; i++) {
        response_time[i] = -1;
    }

    int global_start = uptime();

    for (i = 0; i < NUM_PROCS; i++) {
        pid = fork();
        if (pid == 0) {  // 자식 프로세스
            start_time[i] = uptime();
            for (j = 0; j < NUM_LOOP; j++) {
                if (response_time[i] == -1) {
                    first_run_time[i] = uptime();
                    response_time[i] = first_run_time[i] - start_time[i];
                }
                // CPU-bound 작업
                int k = j * j;
                // I/O-bound 작업 시뮬레이션 (일부 프로세스만)
                if (i % 2 == 0 && j % 1000 == 0) {
                    printf(1, "Proc %d at loop %d\n", i, j);
                    sleep(1);  // I/O 대기 시뮬레이션
                }
            }
            end_time[i] = uptime();
            turnaround_time[i] = end_time[i] - start_time[i];
            printf(1, "[Proc %d] Response Time: %d ticks\n", i, response_time[i]);
            printf(1, "[Proc %d] Turnaround Time: %d ticks\n", i, turnaround_time[i]);
            exit();
        }
    }

    for (i = 0; i < NUM_PROCS; i++) {
        wait();
    }

    int global_end = uptime();
    printf(1, "\nTotal Execution Time: %d ticks\n", global_end - global_start);

    // 평균 계산
    int avg_response = 0, avg_turnaround = 0;
    for (i = 0; i < NUM_PROCS; i++) {
        avg_response += response_time[i];
        avg_turnaround += turnaround_time[i];
    }
    printf(1, "Average Response Time: %d ticks\n", avg_response / NUM_PROCS);
    printf(1, "Average Turnaround Time: %d ticks\n", avg_turnaround / NUM_PROCS);

    exit();
}