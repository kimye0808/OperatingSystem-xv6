#include "types.h"
#include "stat.h"
#include "user.h"

#define NUM_PROCS 6
#define NUM_LOOP 50000

int main(void) {
    int i, j, pid;
    int count[NUM_PROCS][3];
    int start_time[NUM_PROCS], first_run_time[NUM_PROCS], end_time[NUM_PROCS];
    int response_time[NUM_PROCS], turnaround_time[NUM_PROCS];

    for (i = 0; i < NUM_PROCS; i++) {
        count[i][0] = count[i][1] = count[i][2] = 0;
        response_time[i] = -1;
    }

    int global_start = uptime();

    for (i = 0; i < NUM_PROCS; i++) {
        pid = fork();
        if (pid == 0) {
            start_time[i] = uptime();
            for (j = 0; j < NUM_LOOP; j++) {
                int level = getLevel();
                count[i][level]++;
                if (response_time[i] == -1) {
                    first_run_time[i] = uptime();
                    response_time[i] = first_run_time[i] - start_time[i];
                }
                // 작업 분리: 0~3은 CPU-bound, 4~5는 I/O-bound
                if (i < 4) {
                    int k = j * j;  // CPU-bound
                } else if (j % 2000 == 0) {  // I/O bound
                    printf(1, "Proc %d at loop %d\n", i, j);
                    sleep(1);
                }
            }
            end_time[i] = uptime();
            turnaround_time[i] = end_time[i] - start_time[i];
            printf(1, "[Proc %d] L0: %d, L1: %d, L2: %d\n", i, count[i][0], count[i][1], count[i][2]);
            printf(1, "[Proc %d] Response Time: %d ticks\n", i, response_time[i]);
            printf(1, "[Proc %d] Turnaround Time: %d ticks\n", i, turnaround_time[i]);
            exit();
        }
    }

    for (i = 0; i < NUM_PROCS; i++) {
        wait();
    }

    int global_end = uptime();
    int avg_response = 0, avg_turnaround = 0;
    for (i = 0; i < NUM_PROCS; i++) {
        avg_response += response_time[i];
        avg_turnaround += turnaround_time[i];
    }
    printf(1, "\nTotal Execution Time: %d ticks\n", global_end - global_start);
    printf(1, "Average Response Time: %d ticks\n", avg_response / NUM_PROCS);
    printf(1, "Average Turnaround Time: %d ticks\n", avg_turnaround / NUM_PROCS);

    exit();
}