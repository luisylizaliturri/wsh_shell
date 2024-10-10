#ifndef WSH_SHELL_H
#define WSH_SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h> 

typedef enum {
    REDIR_NONE,
    REDIR_INPUT,               // <
    REDIR_OUTPUT,              // >
    REDIR_OUTPUT_APPEND,       // >>
    REDIR_OUTPUT_ERROR,        // &>
    REDIR_OUTPUT_ERROR_APPEND  // &>>
} RedirectionType;

typedef struct Redirection {
    RedirectionType type;
    int fd;       //file descriptor number
    char *file;   //target file
} Redirection;

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

typedef struct History {
    char **commands; //dynamically allocated 
    int count;
    int start;
    int capacity;
} History;

typedef struct ShellVariable {
    char *name;
    char *value;
    struct ShellVariable *next;
} ShellVariable;

//Utilities
static int compare(const void *a, const void *b);
static char *trim(char *line);
static char *read_line(FILE *input_stream);
static char **parse_line(char *line, int *argc, Redirection *redir);

//Helper functions
static RedirectionType get_redirection_type(char *token, int *fd);
static builtin_cmd_t get_builtin_command(char *cmd);
static void add_to_history(char* command);
static void set_shell_var(char *name, char *value);
static char* get_shell_var(char *name);
static void free_history();
static void free_shell_vars();

void execute_vars();
void execute_local(char **args, int argc);
void execute_export(char **args, int argc);
void execute_cd(char **args, int argc);
void execute_exit(int argc);
void execute_history(char **args, int argc);
void execute_ls();

void execute_external_cmd(char **args, char *command_str, int from_history, Redirection *redir);
void execute_builtin_cmd(builtin_cmd_t cmd, char **args, int argc, Redirection *redir);
void run_loop(FILE *input_stream);
int main(int argc, char* argv[]);

#endif //WSH_SHELL_H 
