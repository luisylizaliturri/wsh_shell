#ifndef WSH_SHELL_H
#define WSH_SHELL_H

#include <stdio.h>      
#include <stdlib.h>     
#include <string.h>     //string functions(strcpy, strlen, strcmp,...)
#include <unistd.h>     //POSIX API (fork, execv, dup, dup2)
#include <ctype.h>      //char handling(isspace, isdigit)
#include <sys/types.h>  //datatypes used in sys class
#include <sys/wait.h>   //wait on child processes (wait, waitpid)
#include <dirent.h>     //directory operations (opendir, readdir, closedir)
#include <fcntl.h>      //file control (open, O_RDONLY, O_WRONLY)

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
static int add_to_history(char* command);
static int set_shell_var(char *name, char *value);
static char* get_shell_var(char *name);
static void free_history();
static void free_shell_vars();
int execute_vars();
int execute_local(char **args, int argc);
int execute_export(char **args, int argc);
int execute_cd(char **args, int argc);
void execute_exit(int argc);
int execute_history(char **args, int argc);
int execute_ls();

//Main functions
void execute_external_cmd(char **args, char *command_str, int from_history, Redirection *redir);
void execute_builtin_cmd(builtin_cmd_t cmd, char **args, int argc, Redirection *redir);
void run_loop(FILE *input_stream);
int main(int argc, char* argv[]);

#endif //WSH_SHELL_H 
