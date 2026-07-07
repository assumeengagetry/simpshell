//
// Created by Terminal Void on 2026/7/7.
//

#ifndef SIMPSHELL_BUILTINS_H
#define SIMPSHELL_BUILTINS_H

typedef int (*BuiltinFunc)(char **argv);

BuiltinFunc get_builtin_func(const char *cmd);


#endif //SIMPSHELL_BUILTINS_H
