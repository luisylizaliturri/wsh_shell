#include "./wsh.h"

//private shell variable struct
typedef struct ShellVariable {
    char *name;
    char *value;
    struct ShellVariable *next;
} ShellVariable;

//head of shell variables linked list
ShellVariable *shell_vars_head = NULL;

void set_shell_var(char *name, char *value) {
    ShellVariable*current = shell_vars_head;
    //check if variable exists
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            //update value
            free(current->value);
            current->value = strdup(value);
            if (current->value == NULL) {
                perror("strdup");
                exit(1);
            }
            return;
        }
        current = current->next;
    }
    //variable does not exist yet
    ShellVariable*new_var = malloc(sizeof(ShellVariable));
    if (new_var == NULL) {
        perror("malloc");
        exit(1);
    }
    new_var->name = strdup(name);
    if (new_var->name == NULL) {
        perror("strdup");
        exit(1);
    }
    new_var->value = strdup(value);
    if (new_var->value == NULL) {
        perror("strdup");
        exit(1);
    }
    new_var->next = shell_vars_head;
    shell_vars_head = new_var;
}

char* get_shell_var(char *name) {
    ShellVariable*current = shell_vars_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

void free_shell_vars() {
    ShellVariable*current = shell_vars_head;
    while (current != NULL) {
        ShellVariable*temp = current;
        current = current->next;
        free(temp->name);
        free(temp->value);
        free(temp);
    }
}

int execute_vars(){
    ShellVariable *current = shell_vars_head;
    while (current != NULL) {
        printf("%s=%s\n", current->name, current->value);
        current = current->next;
    }
    return 0;
}

int execute_local(char **args, int argc){
    if (argc != 2) {
        fprintf(stderr, "local: usage: local VAR=value\n");
        return 1;
    }
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');
    if (equal_sign == NULL) {
        fprintf(stderr, "local: invalid argument: %s\n", arg);
        return 1;
    }
    //split variable into name and value
    *equal_sign = '\0';
    char *var = arg;
    char *value = equal_sign + 1;
    set_shell_var(var, value);
    return 0;
}

int execute_export(char **args, int argc){
    printf("Executing command: export\n");
    if (argc != 2) {
        fprintf(stderr, "export: usage: export VAR=value\n");
        return 1;
    }
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');
    if (equal_sign == NULL) {
        fprintf(stderr, "export: invalid argument: %s\n", arg);
        return 1;
    }

    //split variable into name and value
    *equal_sign = '\0';
    char *var = arg;
    char *value = equal_sign + 1;
    if (setenv(var, value, 1) != 0) {
        perror("export");
    }
    return 0;
}

int execute_cd(char **args, int argc){
     if(argc < 2){
        char *home = getenv("HOME");
        if(home == NULL){
            fprintf(stderr, "cd: HOME not set\n");
            return 1;
        }
        if(chdir(home) != 0){
            perror("cd");
        }
    }else if(argc == 2){
        printf("%s", args[1]);
        if(chdir(args[1]) != 0){
            printf("cd error");
            perror("cd");
        }
    }else{
        fprintf(stderr, "cd: too many arguments\n");
    }
    return 0;
}

int execute_exit(int argc){
    printf("Executing command: exit\n");
    if(argc == 1){
        exit(0);
    }
    fprintf(stderr, "exit: too many arguments\n");
    return 1;
}

int execute_external_cmd(char **args){
    pid_t pid; //hold process id of child process
    pid_t wpid;
    int status;

    //fork a child process
    pid = fork();
    if (pid == 0) {
        //execute external commmand 
        if (execvp(args[0], args) == -1) {
            perror("wsh");
        }
        exit(1); //exit on execvp fail
    }
    else if (pid < 0) {
        perror("wsh: fork");
    }else {
        //parent process
        while (1) {
            wpid = waitpid(pid, &status, WUNTRACED);
            if (wpid == -1) {
                perror("wsh: waitpid");
                break;
            }
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                break;
            }
        }
    }
    return 0;
}

int execute_builtin_cmd(builtin_cmd_t cmd, char **args, int argc){
    int status = 1;
    switch(cmd){
        case CMD_EXIT:
            status = execute_exit(argc);
            break;
        case CMD_CD:
            status = execute_cd(args, argc);
            break;
        case CMD_EXPORT:
            status = execute_export(args, argc);
            break;
        case CMD_LOCAL:
            status = execute_local(args, argc);
            break;
        case CMD_VARS:
            status = execute_vars();
            break;
        case CMD_HISTORY:
            //TODO
            break;
        case CMD_LS:
            //TODO
            break;
        default:
            return status;
    }
    return status;
}

builtin_cmd_t get_builtin_command(char *cmd) {
    if (strcmp(cmd, "exit") == 0) return CMD_EXIT;
    if (strcmp(cmd, "cd") == 0) return CMD_CD;
    if (strcmp(cmd, "export") == 0) return CMD_EXPORT;
    if (strcmp(cmd, "local") == 0) return CMD_LOCAL;
    if (strcmp(cmd, "vars") == 0) return CMD_VARS;
    if (strcmp(cmd, "history") == 0) return CMD_HISTORY;
    if (strcmp(cmd, "ls") == 0) return CMD_LS;
    return NOT_BUILT_IN;
}

char *trim(char *line) {
    char *end;
    while (isspace((unsigned char)*line)) {
        line++;
    }
    if (*line == '\0') {
        return line;
    }
    end = line + strlen(line) - 1;
    while (end > line && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
    return line;
}

char **parse_line(char *line, int *argc){
    int buffer_size = 128;
    char **tokens = malloc(buffer_size * sizeof(char*));
    if (!tokens) {
        perror("malloc error");
        exit(1);
    }

    char *token;
    *argc = 0;
    token = strtok(line, " ");
    while(token) {
        tokens[*argc] = token;
        (*argc)++;
        if(*argc >= buffer_size){
            buffer_size += 128;
            tokens = realloc(tokens, buffer_size * sizeof(char*));
            if (!tokens) {
                perror("realloc error");
                exit(1);
            }
        }
        token = strtok(NULL, " ");
    }
    tokens[*argc] = NULL;
    return tokens;
}

char *read_line(FILE *input_stream){
    char *line = NULL;
    size_t buffer_size = 0;

    //read command and dynamically allocate buffer to store the line
    if (getline(&line, &buffer_size, input_stream) == -1){ 
        if(feof(input_stream)) {
            free(line);
            return NULL;//recieved EOF
        }else{
            free(line);
            return NULL;
        }
    }
    return line;
}

void run_loop(FILE *input_stream){
    char *line;
    char **parsed_command;
    int argc;

    //begin prompt loop 
    while(1){
        if(input_stream == stdin){
            printf("wsh> ");
            fflush(stdout);
        }
        line = read_line(input_stream);
        if(line == NULL){
            break; //EOF
        }
        char* trimmed_line = trim(line);
        if(trimmed_line[0] == '#' || trimmed_line[0] == '\0'){
            free(line);
            continue;
        }
        parsed_command = parse_line(trimmed_line, &argc);
        builtin_cmd_t command = get_builtin_command(parsed_command[0]);
        if(command == NOT_BUILT_IN){
            execute_external_cmd(parsed_command);
        }else{
            execute_builtin_cmd(command, parsed_command, argc);
        }
            
        free(parsed_command);
        free(line);
    }
}

int main(int argc, char* argv[]){
    FILE *input_stream = stdin; //default is interactive mode
    if(argc > 2){
        printf("Usage: %s <script_file>\n", argv[0]);
        exit(1);
    }
    if(argc == 2){ //batch mode
        input_stream = fopen(argv[1], "r");
        if(input_stream == NULL){
            perror("Input stream is NULL");
            exit(1);
        }
    }

    //set initial PATH variable
    if(setenv("PATH", "/bin", 1) != 0){
        perror("wsh: setenv");
        exit(1);
    }

    run_loop(input_stream); //main program loop

    if(input_stream != stdin){
        fclose(input_stream);
    }
    return 0;
}