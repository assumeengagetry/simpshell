//
// Created by Terminal Void on 2026/7/7.
//

#include "builtins.h"
#include "jobs.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    char *name;
    int (*func)(char **argv);
} BuiltinCommand;

//Built-ins
static int builtin_cd(char **argv);
static int builtin_exit(char **argv);
static int builtin_pwd(char **argv);
static int builtin_clear(char **argv);
static int builtin_export(char **argv);
static int builtin_exec(char **argv);
static int builtin_source(char **argv);

//Special Built-ins



//Additional Built-ins
static int builtin_ls(char **argv);
static int builtin_mkdir(char **argv);
static int builtin_rmdir(char **argv);
static int builtin_touch(char **argv);
static int builtin_rm(char **argv);

//Job Control Built-ins
int builtin_jobs(char **argv);
int builtin_bg(char **argv);
int builtin_fg(char **argv);
int builtin_wait(char **argv);

BuiltinCommand builtins[] = {
    {"cd", builtin_cd},
    {"exit", builtin_exit},
    {"pwd", builtin_pwd},
    {"clear", builtin_clear},
    {"jobs", builtin_jobs},
    {"bg", builtin_bg},
    {"fg", builtin_fg},
    {NULL, NULL}
};

BuiltinFunc get_builtin_func(const char *cmd) {
    assert(cmd != NULL);

    for (int i = 0; builtins[i].name!=NULL; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return builtins[i].func; // 找到了，把函数的内存地址交出去
        }
    }
    return NULL; // 没找到
}

int builtin_cd(char **argv) {

    if (argv[1]==NULL) {
        const char *home  = getenv("HOME");
        if (home == NULL) {
            fprintf(stderr,"cd: HOME environment variable not set\n");
        }
        else {
            if (chdir(home) != 0) {
                perror("cd");
                return 1;
            }
        }
    }
    else {
        if (chdir(argv[1]) != 0) {
            perror("cd");
            return 1; // 失败返回 1
        }
    }
    return 0;
}

int builtin_exit(char **argv) {

    printf("Bye!\n");
    exit(0); // 直接终止整个 Shell 进程
    //todo: 处理bg进程

}

int builtin_pwd(char **argv) {
    char cwd[1024];
    // getcwd 会把当前绝对路径写入 cwd 数组
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
        return 0; // 退出状态码：成功
    } else {
        // 如果因为某些极端原因（比如当前目录突然被其他程序删除了）导致获取失败
        perror("pwd");
        return 1; // 退出状态码：失败
    }
}

int builtin_clear(char **argv) {
    printf("\033[2J\033[1;1H");
    return 0;
}

int builtin_jobs(char **argv) {
    for (int i = 0; i < MAX_JOBS; i++) {
        Job *job = get_job_by_id(i+1);
        if (job->status != DONE) {
            // 模仿真实 Shell 的输出格式
            printf("[%d] %d %s\t\t%s\n",
                   job->job_id,
                   job->pid,
                   job->status == RUNNING ? "Running" : "Stopped",
                   job->cmd);
        }
    }
    return 0;
}

int builtin_bg(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "bg: current job implementation requires job id\n");
        return 1;
    }

    char *endptr;
    errno = 0;
    const long ljob_id = strtol(argv[1], &endptr, 10);

    if (errno == ERANGE) {
        fprintf(stderr, "bg: %s: job id out of range\n", argv[1]);
        return 1;
    }

    if (endptr == argv[1]) {
        fprintf(stderr, "bg: %s: no such job\n", argv[1]);
        return 1;
    }

    if (*endptr != '\0') {
        fprintf(stderr, "bg: %s: invalid characters in job id\n", argv[1]);
        return 1;
    }

    if (ljob_id <= 0 || ljob_id > MAX_JOBS || ljob_id > INT_MAX) {
        fprintf(stderr, "bg: %s: no such job\n", argv[1]);
        return 1;
    }

    const int job_id = (int)ljob_id;
    const Job *job = get_job_by_id(job_id);

    if (job->status == DONE) {
        fprintf(stderr, "bg: %d: no such job\n", job_id);
        return 1;
    }

    const pid_t target_pid = job->pid;

    if (job->status == STOPPED) {
        if (kill(target_pid, SIGCONT) < 0) {
            perror("bg: kill");
            return 1;
        }
        run_job(job_id);
    }
    printf("[%d] %s &\n", job_id, job->cmd);

    return 0;
}

int builtin_fg(char **argv) {
    // 1. 获取目标 target_pid (同上)
    if (argv[1] == NULL) {
        fprintf(stderr, "fg: current job implementation requires job id\n");
        return 1;
    }
    char *endptr;
    errno = 0;
    const long ljob_id = strtol(argv[1], &endptr, 10);

    if (errno == ERANGE) {
        fprintf(stderr, "fg: %s: job id out of range\n", argv[1]);
        return 1;
    }

    if (endptr == argv[1]) {
        fprintf(stderr, "fg: %s: no such job\n", argv[1]);
        return 1;
    }

    if (*endptr != '\0') {
        fprintf(stderr, "fg: %s: invalid characters in job id\n", argv[1]);
        return 1;
    }

    if (ljob_id <= 0 || ljob_id > MAX_JOBS || ljob_id > INT_MAX) {
        fprintf(stderr, "fg: %s: no such job\n", argv[1]);
        return 1;
    }

    const int job_id = (int)ljob_id;
    const Job *job = get_job_by_id(job_id);

    if (job->status == DONE) {
        fprintf(stderr, "fg: %s: no such job\n", argv[1]);
        return 1;
    }

    const pid_t target_pid = job->pid;
    printf("%s\n", job->cmd);
    if (kill(target_pid, SIGCONT) < 0) {
        perror("fg: kill");
        return 1;
    }

    run_job(job_id);
    int status;
    do {
        waitpid(target_pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

    // 4. 等待结束，如果是正常退出，从 job_table 里把它标记为 DONE
    if (WIFSTOPPED(status)) {
        // 如果是在前台运行时，用户又按了 Ctrl+Z 把它冻结了
        stop_job(job_id);
        // 打印挂起提示
        printf("\n[%d]  + suspended  %s\n", job->job_id, job->cmd);
    } else {
        // 如果是正常跑完，或者被 Ctrl+C 杀掉了
        remove_job(job_id);
    }

    return 0;
}