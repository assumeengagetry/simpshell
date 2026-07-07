//
// Created by Terminal Void on 2026/7/7.
//

#ifndef SIMPSHELL_PARSER_H
#define SIMPSHELL_PARSER_H

int parse_command(char *input, char **cmd_argv, int *is_background);
void expand_tilde(char **argv, char **to_free, int *free_count);

void free_expanded_args(char **to_free, int free_count);

#endif //SIMPSHELL_PARSER_H
