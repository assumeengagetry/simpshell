//
// Created by Terminal Void on 2026/7/7.
//
#include "jobs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

#include "shell.h"

static Job job_table[MAX_JOBS];
static int job_count = 0;

Job *get_job_by_id(const int job_id) {
    const int index = job_id - 1;
    assert(job_id>0 && job_id <= MAX_JOBS);
    return &job_table[index];
}

int create_job(const pid_t pid, const char *cmd) {
    assert(pid > 0);
    assert(cmd != NULL);

    for (int i = 0; i < MAX_JOBS; i++) {
        // 如果这个槽位是空的，或者任务已经 DONE 被清理了
        if (job_table[i].status == DONE) {
            const int job_id = i+1;
            job_table[i].job_id=job_id;
            job_table[i].pid = pid;
            job_table[i].status = RUNNING;
            strncpy(job_table[i].cmd,cmd,sizeof(job_table[i].cmd)-1);
            job_table[i].cmd[sizeof(job_table[i].cmd) - 1] = '\0';
            // 正常情况下不会产生作用，因cmd小于MAX_CMD_LEN，
            // 不会出现strncpy因为cmd长度大于sizeof(job_table[i].cmd)-1而截断不添加\0的情况
            job_count++;
            return job_id;
        }
    }
    return -1; // 表满
}

void run_job(const int job_id) {
    const int index = job_id - 1;
    assert(job_id>0 && job_id <= MAX_JOBS);
    assert(job_table[index].status != DONE);
    job_table[index].status = RUNNING;
}

void remove_job(const int job_id) {
    const int index = job_id - 1;
    assert(job_id>0 && job_id <= MAX_JOBS);
    assert(job_table[index].status != DONE);
    job_table[index].status = DONE;
    job_count--;

}

void stop_job(const int job_id) {
    const int index = job_id - 1;
    assert(job_id > 0 && job_id <= MAX_JOBS);
    assert(job_table[index].status != DONE);
    job_table[index].status = STOPPED;
}

void check_background_jobs() {
    int status;
    pid_t dead_pid;

    while ((dead_pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // 查表：死掉的这个 PID 对应哪个 Job？
        for (int i = 0; i < MAX_JOBS; i++) {
            // DONE 就是我们定义的 0 (EMPTY)
            if (job_table[i].status != DONE && job_table[i].pid == dead_pid) {

                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    // 任务死透了，打印通知
                    printf("[%d] Done    %s\n", i + 1, job_table[i].cmd);
                    remove_job(i+1);
                }
                else if (WIFSTOPPED(status)) {
                    stop_job(i+1);
                    printf("\n[%d]  + suspended  %s\n", i + 1, job_table[i].cmd);
                }
                break; // 找到了就跳出 for 循环，继续 while 查下一个
            }
        }
    }
}