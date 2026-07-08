//
// Created by Terminal Void on 2026/7/7.
//

#ifndef SIMPSHELL_JOBS_H
#define SIMPSHELL_JOBS_H
#include "shell.h"
#include <sys/types.h>

typedef enum {
    DONE=0,
    RUNNING,
    STOPPED
} JobStatus;

typedef struct {
    int job_id;
    pid_t pgid;
    char cmd[MAX_CMD_LEN];
    JobStatus status;
} Job;

//Job Control

int create_job(pid_t pgid, const char *cmd);
void remove_job(int job_id);
void run_job(int job_id);
void stop_job(int job_id);
void check_background_jobs(void);

Job *get_job_by_id(int job_id);
int get_job_count(void);

#endif //SIMPSHELL_JOBS_H
