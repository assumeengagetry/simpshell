//
// Created by Terminal Void on 2026/7/7.
//

#include "executor.h"
#include "jobs.h"
#include "shell.h"

#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

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
                    stop_job(job_id); // 建档后强制覆盖状态为暂停
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
