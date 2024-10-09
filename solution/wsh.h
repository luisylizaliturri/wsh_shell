#ifndef WSH_SHELL_H
#define WSH_SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef enum {
    CMD_EXIT,
    CMD_CD,
    CMD_EXPORT,
    CMD_LOCAL,
    CMD_VARS,
    CMD_HISTORY,
    CMD_LS,
    NOT_BUILT_IN
} builtin_cmd_t;

int main(int argc, char* argv[]); //call run_loop 
void run_loop();
char* read_line();
char* parse_command();
void execute_command();

#endif //WSH_SHELL_H 
