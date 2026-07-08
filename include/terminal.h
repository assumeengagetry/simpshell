//
// Created by Terminal Void on 2026/7/7.
//

#ifndef SIMPSHELL_TERMINAL_H
#define SIMPSHELL_TERMINAL_H

#include <sys/types.h>
#include <termios.h>

void init_shell(void);

int get_shell_terminal(void);
pid_t get_shell_pgid(void);
int is_shell_interactive(void);
const struct termios *get_shell_tmodes(void);

int give_terminal_to(pid_t pgid);
int reclaim_terminal(void);

#endif //SIMPSHELL_TERMINAL_H
