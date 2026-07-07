//
// Created by Terminal Void on 2026/7/7.
//

#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "shell.h"

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

void free_expanded_args(char **to_free, int free_count) {
    assert(to_free != NULL);
    for (int i = 0; i < free_count; i++) {
        free(to_free[i]);
    }

}