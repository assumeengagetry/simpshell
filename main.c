#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include<assert.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define MAX_JOBS 100
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef int (*BuiltinFunc)(char **argv);

typedef enum {
    DONE=0,
    RUNNING,
    STOPPED
} JobStatus;

typedef struct {
    int job_id;
    pid_t pid;
    char cmd[MAX_CMD_LEN];
    JobStatus status;
} Job;

typedef struct {
    char *name;
    int (*func)(char **argv);
} BuiltinCommand;

Job job_table[MAX_JOBS];
int job_count = 0;      // 记录当前有多少个后台任务


int parse_command(char *input, char **cmd_argv, int *is_background);
void expand_tilde(char **argv, char **to_free, int *free_count);
BuiltinFunc get_builtin_func(const char *cmd);
int execute_external(char **argv, int is_background);

//Built-ins
int builtin_cd(char **argv);
int builtin_exit(char **argv);
int builtin_pwd(char **argv);
int builtin_clear(char **argv);
int builtin_export(char **argv);
int builtin_exec(char **argv);
int builtin_source(char **argv);

//Special Built-ins

//Job Control
int builtin_jobs(char **argv);
int builtin_bg(char **argv);
int builtin_fg(char **argv);
int builtin_wait(char **argv);
void check_background_jobs();
int create_job(pid_t pid, const char *cmd);
void remove_job(int job_id);
void stop_job(int job_id);




//Additional Built-ins
int builtin_ls(char **argv);
int builtin_mkdir(char **argv);
int builtin_rmdir(char **argv);
int builtin_touch(char **argv);
int builtin_rm(char **argv);

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


int main(int argc, const char *argv[]) {
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    char input[MAX_CMD_LEN];
    char *cmd_argv[MAX_ARGS];
    char cwd[1024];

    while (1) {

        check_background_jobs();

        //0. 准备有关变量
        int free_count = 0;
        char *to_free[MAX_ARGS];
        int is_background = 0;

        // 1. 打印Prompt
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            const char *home = getenv("HOME");
            if (home != NULL) {
                const size_t home_len = strlen(home);
                if (strcmp(cwd, home) == 0) {
                    printf("SimpShell:~ %% ");
                } else if (strncmp(cwd, home, home_len) == 0 && cwd[home_len] == '/') {
                    printf("SimpShell:~%s %% ", cwd + home_len);
                } else {
                    printf("SimpShell:%s %% ", cwd);
                }
            }
            else {
                printf("SimpShell:%s %% ", cwd);
            }

        } else {
            // 如果获取失败（比如目录权限被意外剥夺），退化为默认提示符
            perror("getcwd");
            printf("SimpShell %% ");
        }

        fflush(stdout); // 刷新缓冲区

        // 2. 读取用户输入
        if (fgets(input, MAX_CMD_LEN, stdin) == NULL) {
            // 处理按下 Ctrl+D (EOF) 的情况：优雅退出
            printf("\n");
            break;
        }

        // 3. 去除字符串末尾的换行符
        input[strcspn(input, "\n")] = '\0';

        // 如果用户什么都没输直接按了回车，跳过本次循环
        if (strlen(input) == 0) { continue; }


        const int cmd_argc = parse_command(input, cmd_argv , &is_background);
        if (cmd_argc == 0) {
            continue;
        }
        expand_tilde(cmd_argv, to_free, &free_count);
        const BuiltinFunc builtin_func = get_builtin_func(cmd_argv[0]);

        if (builtin_func != NULL) {
            int status = builtin_func(cmd_argv);
        }
        else {
            execute_external(cmd_argv,is_background);
        }

        // 释放内存
        for (int i = 0; i < free_count; i++) {
            free(to_free[i]);
        }
        free_count=0;
    }
    return 0;
}

BuiltinFunc get_builtin_func(const char *cmd) {
    assert(cmd != NULL);

    for (int i = 0; builtins[i].name!=NULL; i++) {
        if (strcmp(cmd, builtins[i].name) == 0) {
            return builtins[i].func; // 找到了，把函数的内存地址交出去
        }
    }
    return NULL; // 没找到
}

int parse_command(char *input, char **cmd_argv, int *is_background) {
    assert(input != NULL);
    assert(cmd_argv != NULL);
    assert(is_background != NULL);

    int argc = 0;
    *is_background = 0;

    char *token = strtok(input, " \t");

    while (token != NULL) {
        if (argc >= MAX_ARGS - 1) {
            fprintf(stderr, "shell: too many arguments\n");
            break;
        }
        cmd_argv[argc++] = token;
        token = strtok(NULL, " \t");
    }
    cmd_argv[argc] = NULL;

    if (argc == 0) {
        return 0;
    }

    if (strcmp(cmd_argv[argc - 1], "&") == 0) {
        *is_background = 1;
        cmd_argv[argc - 1] = NULL;
        argc--;
    }

    return argc;
}

void expand_tilde(char **argv, char **to_free, int *free_count) {
    const char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr,"expand_tilde: HOME environment variable not set\n");
        return;
    }

    for (int i=0; argv[i] != NULL; i++) {
        if (strcmp(argv[i], "~") == 0 || strncmp(argv[i], "~/", 2) == 0) {
            char buffer[1024];
            snprintf(buffer, sizeof(buffer), "%s%s", home, argv[i]+1);

            char *expanded = strdup(buffer); // 需要手动free
            if (expanded == NULL) {
                perror("strdup");
                continue;
            }

            argv[i] = expanded;
            to_free[*free_count] = argv[i];
            (*free_count)++;
        }
    }
}

int execute_external(char **argv,const int is_background) {
    assert(argv != NULL);
    assert(argv[0] != NULL);
    const pid_t pid = fork();
    if (pid < 0) {
        // 情况 A：克隆失败（极其罕见，通常是系统内存爆了，或者进程数达到了上限）
        perror("fork failed");
        return 1;
    }
    if (pid == 0) {// Sub process

        signal(SIGINT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);

        execvp(argv[0], argv);

        //如果运行到此处，说明execvp失败，子进程自杀
        perror(argv[0]);
        exit(EXIT_FAILURE);
    }
    else { //Main process
        char cmd_string[1024] = {0};
        size_t offset = 0;
        for (int i = 0; argv[i] != NULL; i++) {
            const size_t arg_len = strlen(argv[i]);
            if (offset + arg_len + 1 + 1 >= sizeof(cmd_string)) {
                break;
            }
            memcpy(cmd_string + offset, argv[i], arg_len);
            offset += arg_len;
            if (argv[i + 1] != NULL) {
                cmd_string[offset++] = ' ';
            }
        }
        cmd_string[offset] = '\0';

        if (!is_background) {

            int status;
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

            if (WIFSTOPPED(status)) { //CTRL+Z

                const int job_id = create_job(pid, cmd_string);
                if (job_id != -1) {
                    job_table[job_id - 1].status = STOPPED; // 建档后强制覆盖状态为暂停
                    printf("\n[%d]  + suspended  %s\n", job_id, cmd_string);
                }
            }

        }
        else {
            const int job_id = create_job(pid, cmd_string);
            if (job_id != -1) {
                printf("[%d] %d\n", job_id, pid);
            } else {
                fprintf(stderr, "Shell: maximum number of jobs exceeded\n");
            }


        }
    }

    return 0;
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

int builtin_jobs(char **argv) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (job_table[i].status != DONE) {
            // 模仿真实 Shell 的输出格式
            printf("[%d] %d %s\t\t%s\n",
                   job_table[i].job_id,
                   job_table[i].pid,
                   job_table[i].status == RUNNING ? "Running" : "Stopped",
                   job_table[i].cmd);
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
    const int index = job_id - 1;
    if (job_table[index].status == DONE) {
        fprintf(stderr, "bg: %d: no such job\n", job_id);
        return 1;
    }

    const pid_t target_pid = job_table[index].pid;

    if (job_table[index].status == STOPPED) {
        if (kill(target_pid, SIGCONT) < 0) {
            perror("bg: kill");
            return 1;
        }
        run_job(job_id);
    }
    printf("[%d] %s &\n", job_id, job_table[index].cmd);

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
    const int index = job_id - 1;

    if (job_table[index].status == DONE) {
        fprintf(stderr, "fg: %s: no such job\n", argv[1]);
        return 1;
    }

    const pid_t target_pid = job_table[index].pid;
    printf("%s\n", job_table[index].cmd);
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
        printf("\n[%d]  + suspended  %s\n", job_table[index].job_id, job_table[index].cmd);
    } else {
        // 如果是正常跑完，或者被 Ctrl+C 杀掉了
        remove_job(job_id);
    }

    return 0;
}