#include "./wsh.h"

#define HISTORY_SIZE 5
#define MAX_CMD_SIZE 128

//Globals
static ShellVariable *g_shell_vars_head = NULL; //head of shell vars linked list
static History g_history = {.commands = {NULL}, .count = 0, .start = 0}; //stores recent command history

//Utilities
static char *trim(char *line) {
    char *end;
    while(isspace((unsigned char)*line)){
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

static char *read_line(FILE *input_stream){
    char *line = NULL;
    size_t buffer_size = 0;

    //read command and dynamically allocate buffer to store the line
    if (getline(&line, &buffer_size, input_stream) == -1){ 
        free(line);
        return NULL;
    }
    return line;
}

static char **parse_line(char *line, int *argc){
    int buffer_size = MAX_CMD_SIZE;
    char **tokens = malloc(buffer_size * sizeof(char*));
    if (!tokens) {
        perror("malloc error");
        exit(1);
    }

    char *token;
    *argc = 0;
    token = strtok(line, " ");
    while(token) {
        if(token[0] == '$'){
            char *name = token +1;
            char *value = getenv(name);
            if(value == NULL){
                value = get_shell_var(name);
            }
            if(value != NULL){
                tokens[*argc] = strdup(value);
                if(tokens[*argc] == NULL){
                    perror("strdup");
                    exit(1);
                }
            }else{
                tokens[*argc] = strdup("");
                if(tokens[*argc] ==NULL){
                    perror("strdup");
                    exit(1);
                }
            }
        }else{
            tokens[*argc] = strdup(token);
            if(tokens[*argc] == NULL){
                perror("strdup");
                exit(1);
            }
        }
        (*argc)++;
        if(*argc >= buffer_size){
            buffer_size += MAX_CMD_SIZE;
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

//Helper functions
static builtin_cmd_t get_builtin_command(char *cmd) {
    if (strcmp(cmd, "exit") == 0) return CMD_EXIT;
    if (strcmp(cmd, "cd") == 0) return CMD_CD;
    if (strcmp(cmd, "export") == 0) return CMD_EXPORT;
    if (strcmp(cmd, "local") == 0) return CMD_LOCAL;
    if (strcmp(cmd, "vars") == 0) return CMD_VARS;
    if (strcmp(cmd, "history") == 0) return CMD_HISTORY;
    if (strcmp(cmd, "ls") == 0) return CMD_LS;
    return NOT_BUILT_IN;
}

static void add_to_history(char* command){
    if(g_history.count > 0){
        int last_index = (g_history.start + g_history.count - 1) % HISTORY_SIZE;
        if (strcmp(g_history.commands[last_index], command) == 0) {
            return;
        }
    }
    //history is full, remove the oldest command
    if (g_history.count == HISTORY_SIZE) {
        free(g_history.commands[g_history.start]);
        g_history.commands[g_history.start] = strdup(command);
        if (g_history.commands[g_history.start] == NULL) {
            perror("strdup");
            exit(1);
        }
        g_history.start = (g_history.start + 1) % HISTORY_SIZE;
    }else {
        g_history.commands[(g_history.start + g_history.count) % HISTORY_SIZE] = strdup(command);
        if (g_history.commands[(g_history.start + g_history.count) % HISTORY_SIZE] == NULL) {
            perror("strdup");
            exit(1);
        }
        g_history.count++;
    }    
}

static void set_shell_var(char *name, char *value){
    ShellVariable *current = g_shell_vars_head;
    //check if variable exists
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            char *new_value = strdup(value);
            if (new_value == NULL) {
                perror("strdup");
                exit(EXIT_FAILURE);
            }
            free(current->value);
            current->value = new_value;
            return;
        }
        current = current->next;
    }
    //variable does not exist yet
    ShellVariable *new_var = malloc(sizeof(ShellVariable));
    if (new_var == NULL) {
        perror("malloc");
        exit(1);
    }
    new_var->name = strdup(name);
    if (new_var->name == NULL) {
        perror("strdup");
        free(new_var);
        exit(1);
    }
    new_var->value = strdup(value);
    if (new_var->value == NULL) {
        perror("strdup");
        free(new_var->name);
        free(new_var);
        exit(1);
    }
    new_var->next = g_shell_vars_head;
    g_shell_vars_head = new_var;
}

static char* get_shell_var(char *name){
    ShellVariable*current = g_shell_vars_head;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return NULL;
}

static void free_history(){
    for(int i =0; i < g_history.count ; i++){
        int index = (g_history.start + i) % HISTORY_SIZE;
        free(g_history.commands[index]);
    }
}

static void free_shell_vars(){
    ShellVariable* current = g_shell_vars_head;
    while (current != NULL) {
        ShellVariable*temp = current;
        current = current->next;
        free(temp->name);
        free(temp->value);
        free(temp);
    }
}
//Execute builtin commands
void execute_vars(){
    ShellVariable *current = g_shell_vars_head;
    while (current != NULL) {
        printf("%s=%s\n", current->name, current->value);
        current = current->next;
    }
}

void execute_local(char **args, int argc){
    if (argc != 2) {
        fprintf(stderr, "local: usage: local VAR=value\n");
        return;
    }
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');
    if (equal_sign == NULL) {
        fprintf(stderr, "local: invalid argument: %s\n", arg);
        return;
    }
    //split variable into name and value
    *equal_sign = '\0';
    char *var = arg;
    char *value = equal_sign + 1;
    set_shell_var(var, value);
    return;
}

void execute_export(char **args, int argc){
    printf("Executing command: export\n");
    if (argc != 2) {
        fprintf(stderr, "export: usage: export VAR=value\n");
        return;
    }
    char *arg = args[1];
    char *equal_sign = strchr(arg, '=');
    if (equal_sign == NULL) {
        fprintf(stderr, "export: invalid argument: %s\n", arg);
        return;
    }

    //split variable into name and value
    *equal_sign = '\0';
    char *var = arg;
    char *value = equal_sign + 1;
    if (setenv(var, value, 1) != 0) {
        perror("export");
    }
    return;
}

void execute_cd(char **args, int argc){
     if(argc < 2){
        char *home = getenv("HOME");
        if(home == NULL){
            fprintf(stderr, "cd: HOME not set\n");
            return;
        }
        if(chdir(home) != 0){
            perror("cd");
            exit(1);
        }
    }else if(argc == 2){
        if(chdir(args[1]) != 0){
            perror("cd error");
            exit(1);
        }
    }else{
        fprintf(stderr, "cd: too many arguments\n");
        return;
    }
}

void execute_exit(int argc){
    if(argc == 1){
        free_shell_vars();
        free_history();
        return;
    }
    fprintf(stderr, "exit: too many arguments\n");
    return;
}

void execute_history(char **args, int argc){
    if(argc > 3){
        fprintf(stderr, "history: too many arguments\n");
        return;
    }
    
    if(argc == 3 && strcmp(args[1], "set") == 0){
        int size = atoi(args[2]);
        if(size <= 0){
            fprintf(stderr, "history: set: invalid size: %s\n", args[2]);
            return;
        }

        //TODO: implement set size

    }else if(argc == 2){
        int command_num = atoi(args[1]);
        if(command_num <= 0 || command_num > g_history.count){
            fprintf(stderr, "history: %d: event not found\n", command_num);
            return;
        }
        int index = (g_history.start + g_history.count - command_num) % HISTORY_SIZE;
        char *command_str = g_history.commands[index];
        printf("%s\n", command_str);
        char *command_str_copy = strdup(command_str);
        if(command_str_copy == NULL) {
            perror("strdup");
            free_history();
            free_shell_vars();
            exit(1);
        }

        int arg_count = 0;
        char **parsed_command = parse_line(command_str, &arg_count);
        if(parsed_command == NULL){
            fprintf(stderr, "history: parse_line failed\n");
            free(command_str_copy);
            return;
        }
        if(parsed_command[0] != NULL){
            builtin_cmd_t command = get_builtin_command(parsed_command[0]);
            if(command == NOT_BUILT_IN){
                execute_external_cmd(parsed_command, command_str_copy, 1);
            }else{
                execute_builtin_cmd(command, parsed_command, arg_count);
            }
        }else{
            perror("parse_line");
            exit(1);
        }
        for(int i = 0; i < arg_count; i++) {
            free(parsed_command[i]);
        }
        free(parsed_command);
        free(command_str_copy);
    }else if(argc == 1){
        for(int i = 0; i < g_history.count; i++){
            int index = (g_history.start + g_history.count - 1 - i) % HISTORY_SIZE;
            printf("%d) %s\n", i +1, g_history.commands[index]);
        }
    }
}

void execute_ls(){
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // ignore "." and ".."
            if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
                continue;
            }
            printf("%s\n", dir->d_name);
        }
        closedir(d);
    }else {
        perror("ls");
        exit(1);
    }
}

//Main functions
void execute_external_cmd(char **args, char *command_str, int from_history){
    pid_t pid; //hold process id of child process
    pid_t wpid;
    int status;

    //fork a child process
    pid = fork();
    if (pid == 0) {
        //execute external commmand 
        if (execvp(args[0], args) == -1) {
            perror("wsh");
            return;
        }
        exit(1); //exit on execvp fail
    }else if(pid < 0){
        perror("wsh: fork");
        return;
    }else{
        //parent process
        while (1) {
            wpid = waitpid(pid, &status, WUNTRACED);
            if (wpid == -1) {
                perror("wsh: waitpid");
                return;
            }
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                break;
            }
        }
    }
    if(!from_history){
        add_to_history(command_str);
    }
}

void execute_builtin_cmd(builtin_cmd_t cmd, char **args, int argc){
    switch(cmd){
        case CMD_EXIT:
            execute_exit(argc);
            break;
        case CMD_CD:
            execute_cd(args, argc);
            break;
        case CMD_EXPORT:
            execute_export(args, argc);
            break;
        case CMD_LOCAL:
            execute_local(args, argc);
            break;
        case CMD_VARS:
            execute_vars();
            break;
        case CMD_HISTORY:
            execute_history(args, argc);
            break;
        case CMD_LS:
            execute_ls();
            break;
        default:
            return;
    }
    return;
}

void run_loop(FILE *input_stream){
    char *line;
    char *command_str_copy;
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

        command_str_copy = strdup(trimmed_line);
        if(command_str_copy == NULL){
            perror("strdup");
            free(line);
            exit(1);
        }

        parsed_command = parse_line(trimmed_line, &argc);
        builtin_cmd_t command = get_builtin_command(parsed_command[0]);

        if(command == CMD_EXIT){
            //free before exit
            execute_exit(argc);
            for(int i =0; i < argc; i++) {
                free(parsed_command[i]);
            }
            free(command_str_copy);
            free(parsed_command);
            free(line);
            exit(0);
        }else if(command == NOT_BUILT_IN){
            execute_external_cmd(parsed_command,command_str_copy, 0);
            free(command_str_copy);
        }else{
            free(command_str_copy);
            execute_builtin_cmd(command, parsed_command, argc);
        }

        //free each token
        for(int i =0; i < argc; i++) {
            free(parsed_command[i]);
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