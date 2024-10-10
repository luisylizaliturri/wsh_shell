#include "./wsh.h"

#define DEFAULT_HISTORY_SIZE 5
#define MAX_CMD_SIZE 128

//Globals
static ShellVariable *g_shell_vars_head = NULL; //head of shell vars linked list
static History g_history = {.commands = NULL, .count = 0, .start = 0}; //stores recent command history

//Utilities
static int compare(const void *a, const void *b){
    const char **str_a = (const char **)a;
    const char **str_b = (const char **)b;
    return strcmp(*str_a, *str_b);
}

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

static char **parse_line(char *line, int *argc, Redirection *redir) {
    int buffer_size = MAX_CMD_SIZE;
    char **tokens = malloc(buffer_size * sizeof(char*));
    if (!tokens) {
        perror("malloc error");
        exit(1);
    }

    // Initialize redirection structure
    redir->type = REDIR_NONE;
    redir->fd = STDOUT_FILENO;  // Default to stdout
    redir->file = NULL;

    char *token;
    *argc = 0;
    token = strtok(line, " ");
    while (token) {
        // Check for variable substitution
        if (token[0] == '$') {
            char *name = token + 1;
            char *value = getenv(name);
            if (value == NULL) {
                value = get_shell_var(name);
            }
            if (value != NULL) {
                tokens[*argc] = strdup(value);
                if (tokens[*argc] == NULL) {
                    perror("strdup");
                    exit(1);
                }
            } else {
                tokens[*argc] = strdup("");
                if (tokens[*argc] == NULL) {
                    perror("strdup");
                    exit(1);
                }
            }
        }else{// Check for redirection with file descriptor
            int fd = -1;
            char *redir_operator = NULL;
            redir->type = get_redirection_type(token, &fd);

            //Extract file
            if ((redir_operator = strstr(token, ">>")) != NULL) {
                redir->file = strdup(redir_operator + 2); 
            } else if ((redir_operator = strstr(token, ">")) != NULL) {
                redir->file = strdup(redir_operator + 1); 
            } else if ((redir_operator = strstr(token, "&>>")) != NULL) {
                redir->file = strdup(redir_operator + 3);
            } else if ((redir_operator = strstr(token, "&>")) != NULL) {
                redir->file = strdup(redir_operator + 2); 
            } else if ((redir_operator = strstr(token, "<")) != NULL) {
                redir->file = strdup(redir_operator + 1); 
            }

            if (redir_operator != NULL) {
                if (redir->file == NULL || strlen(redir->file) == 0) {
                    fprintf(stderr, "wsh: syntax error near unexpected token '%s'\n", token);
                    break;
                }

                redir->fd = (fd == -1) ? STDOUT_FILENO : fd;
                //continue;  //Dont add to arguments
            }else{
                tokens[*argc] = strdup(token);
                if (tokens[*argc] == NULL) {
                    perror("strdup");
                    exit(1);
                }
            }
        }

        (*argc)++;
        if(*argc >= buffer_size) {
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

static RedirectionType get_redirection_type(char *token, int *fd){
    char *redir_pos = token;

    //extract the file descriptor, if any
    while (isdigit(*redir_pos)) {
        redir_pos++;  // Move pointer past the digits
    }

    if (redir_pos != token) { //fd is found
        *fd = atoi(token);
    }

    // Now redir_pos points to the redirection operator (e.g., '>', '>>', '&>')
    if (strncmp(redir_pos, "&>>", 3) == 0) {
        return REDIR_OUTPUT_ERROR_APPEND;
    }
    if (strncmp(redir_pos, "&>", 2) == 0) {
        return REDIR_OUTPUT_ERROR;
    }
    if (strncmp(redir_pos, ">>", 2) == 0) {
        return REDIR_OUTPUT_APPEND;
    }
    if (strncmp(redir_pos, ">", 1) == 0) {
        return REDIR_OUTPUT;
    }
    if (strncmp(redir_pos, "<", 1) == 0) {
        return REDIR_INPUT;
    }

    // No redirection found
    return REDIR_NONE;
}

static void init_history(){
    g_history.commands = malloc(DEFAULT_HISTORY_SIZE * sizeof(char *));
    if (g_history.commands == NULL) {
        perror("malloc");
        exit(1);
    }
    g_history.count = 0;
    g_history.start = 0;
    g_history.capacity = DEFAULT_HISTORY_SIZE;
}

static void set_history_size(int new_size){
    if (new_size <= 0) {
        fprintf(stderr, "history: set: invalid size: %d\n", new_size);
        return;
    }

    char **new_commands = malloc(new_size * sizeof(char *));
    if (new_commands == NULL) {
        perror("malloc");
        return;
    }

    //Shrinking history size
    int keep_count;
    if(g_history.count < new_size){
        keep_count = g_history.count;
    }else{
        keep_count = new_size;
    }

    //Copy most recent commands to new histoy list
    int curr_start_index = (g_history.start + g_history.count - keep_count) % g_history.capacity;
    for(int i = 0; i < keep_count; i++) {
        new_commands[i] = g_history.commands[(curr_start_index + i) % g_history.capacity];
    }

    // // Free any commands that are being discarded (if reducing size)
    // if (new_size < g_history.count) {
    //     for(int i = keep_count; i < g_history.count; i++) {
    //         // These commands have already been moved, so nothing to free
    //         // If you had pointers to additional structures, free them here
    //     }
    // }

    //free old history
    free(g_history.commands);

    //update history
    g_history.commands = new_commands;
    g_history.capacity = new_size;
    g_history.start = 0;
    g_history.count = keep_count;
}

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

    //check for contiguous duplicate
    if(g_history.count > 0){
        int last_index = (g_history.start + g_history.count - 1) % g_history.capacity;
        if (strcmp(g_history.commands[last_index], command) == 0) {
            return;
        }
    }

    //history is full, remove the oldest command
    if (g_history.count == g_history.capacity) {
        free(g_history.commands[g_history.start]);
        g_history.commands[g_history.start] = strdup(command);
        if (g_history.commands[g_history.start] == NULL) {
            perror("strdup");
            exit(1);
        }
        g_history.start = (g_history.start + 1) % g_history.capacity;
    }else {
        int index = (g_history.start + g_history.count) % g_history.capacity;
        g_history.commands[index]= strdup(command);
        if (g_history.commands[index] == NULL) {
            perror("strdup");
            exit(1);
        }
        g_history.count++;
    }    
}

static void set_shell_var(char *name, char *value) {
    ShellVariable *current = g_shell_vars_head;

    // Check if the variable already exists in the list
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            // Variable exists, update the value
            char *new_value = strdup(value);
            if (new_value == NULL) {
                perror("strdup");
                exit(1);
            }
            free(current->value);
            current->value = new_value;
            return;
        }
        current = current->next;
    }

    // Create a new ShellVariable
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
    new_var->next = NULL;

    if (g_shell_vars_head == NULL) {
        g_shell_vars_head = new_var;
    } else {
        ShellVariable *tail = g_shell_vars_head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = new_var;
    }
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
        int index = (g_history.start + i) % g_history.capacity;
        free(g_history.commands[index]);
    }
    free(g_history.commands);
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
    if(equal_sign == NULL) {
        fprintf(stderr, "local: invalid argument: %s\n", arg);
        return;
    }
    //split variable into name and value
    *equal_sign = '\0';
    char *name = arg;
    char *value = equal_sign + 1;
    if(value[0] == '$'){
        char *var_name = value +1;
        char *var_value = getenv(var_name);
        if(var_value == NULL){
            var_value = get_shell_var(var_name);
        }
        if(var_value != NULL){
            value = strdup(var_value);
            if(value == NULL){
                perror("strdup");
                exit(1);
            }
        }else{
            value = strdup("");
            if(value ==NULL){
                perror("strdup");
                exit(1);
            }
        }
    }
    set_shell_var(name, value);
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
        set_history_size(size);
        return;
    }else if(argc == 2){
        int command_num = atoi(args[1]);
        if(command_num <= 0 || command_num > g_history.count){
            fprintf(stderr, "history: %d: event not found\n", command_num);
            return;
        }
        int index = (g_history.start + g_history.count - command_num) % g_history.capacity;
        char *command_str = g_history.commands[index];
        printf("%s\n", command_str);
        char *command_str_copy = strdup(command_str);
        if(command_str_copy == NULL) {
            perror("strdup");
            free_history();
            free_shell_vars();
            exit(1);
        }
        Redirection redir;
        int arg_count = 0;
        char **parsed_command = parse_line(command_str, &arg_count, &redir);
        if(parsed_command == NULL){
            fprintf(stderr, "history: parse_line failed\n");
            free(command_str_copy);
            return;
        }
        if(parsed_command[0] != NULL){
            builtin_cmd_t command = get_builtin_command(parsed_command[0]);
            if(command == NOT_BUILT_IN){
                execute_external_cmd(parsed_command, command_str_copy, 1, &redir);
            }else{
                execute_builtin_cmd(command, parsed_command, arg_count, &redir);
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
            int index = (g_history.start + g_history.count - 1 - i) % g_history.capacity;
            printf("%d) %s\n", i +1, g_history.commands[index]);
        }
    }
}

void execute_ls() {
    DIR *d;
    struct dirent *dir;
    char **filenames = NULL;
    size_t count = 0;
    size_t capacity = 10;
    d = opendir(".");
    if (!d) {
        perror("ls");
        exit(1);
    }
    filenames = malloc(capacity * sizeof(char *));
    if (filenames == NULL) {
        perror("malloc");
        closedir(d);
        exit(1);
    }
    while ((dir = readdir(d)) != NULL) {
        // ignore "." and ".."
        if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) {
            continue;
        }
        if (dir->d_name[0] == '.') {
            continue;
        }
        if (count >= capacity) {
            capacity *= 2;
            filenames = realloc(filenames, capacity * sizeof(char *));
            if (filenames == NULL) {
                perror("realloc");
                closedir(d);
                exit(1);
            }
        }
        filenames[count] = strdup(dir->d_name);
        if (filenames[count] == NULL) {
            perror("strdup");
            closedir(d);
            exit(1);
        }

        count++;
    }
    qsort(filenames, count, sizeof(char *), compare);

    for (size_t i = 0; i < count; i++) {
        printf("%s\n", filenames[i]);
        free(filenames[i]); 
    }
    free(filenames);
    closedir(d);
}

//Main functions
void execute_external_cmd(char **args, char *command_str, int from_history, Redirection *redir){
    pid_t pid; // Process ID of the child process
    pid_t wpid;
    int status;
    char *path_env = getenv("PATH");
    char *path = NULL;
    int saved_stdout = -1;
    int saved_stdin = -1;
    int saved_stderr = -1;
    int fd;

    // If PATH is not found, fall back to "/bin"
    if (!path_env) {
        path_env = "/bin";
    }

    // Tokenize PATH environment variable to get the list of directories
    char *token = strtok(path_env, ":");
    while (token != NULL) {
        // Allocate enough space for the directory, slash, and the command
        path = malloc(strlen(token) + strlen(args[0]) + 2);  // +2 for '/' and '\0'
        if (!path) {
            perror("malloc");
            return;
        }

        // Construct the full path to the executable
        sprintf(path, "%s/%s", token, args[0]);

        // Check if the file exists and is executable
        if (access(path, X_OK) == 0) {
            break;  // Found the executable
        }

        // Free and try the next directory
        free(path);
        path = NULL;
        token = strtok(NULL, ":");
    }

    if (path == NULL) {
        fprintf(stderr, "wsh: command not found: %s\n", args[0]);
        return;
    }

    // Fork a child process
    pid = fork();
    if (pid == 0) {
        // Child process: handle redirection before executing the command

        // If there is any redirection, handle it
        if (redir->type != REDIR_NONE) {
            if (redir->type == REDIR_OUTPUT || redir->type == REDIR_OUTPUT_APPEND) {
                fd = open(redir->file, O_WRONLY | O_CREAT | (redir->type == REDIR_OUTPUT_APPEND ? O_APPEND : O_TRUNC), 0644);
                if (fd < 0) {
                    perror("open");
                    exit(1);
                }
                // Save current stdout
                saved_stdout = dup(STDOUT_FILENO);
                // Redirect stdout to the file
                if (dup2(fd, STDOUT_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    exit(1);
                }
                close(fd); // No need to keep this fd open anymore
            } else if (redir->type == REDIR_INPUT) {
                fd = open(redir->file, O_RDONLY);
                if (fd < 0) {
                    perror("open");
                    exit(1);
                }
                // Save current stdin
                saved_stdin = dup(STDIN_FILENO);
                // Redirect stdin to the file
                if (dup2(fd, STDIN_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    exit(1);
                }
                close(fd); // No need to keep this fd open anymore
            } else if (redir->type == REDIR_OUTPUT_ERROR || redir->type == REDIR_OUTPUT_ERROR_APPEND) {
                fd = open(redir->file, O_WRONLY | O_CREAT | (redir->type == REDIR_OUTPUT_ERROR_APPEND ? O_APPEND : O_TRUNC), 0644);
                if (fd < 0) {
                    perror("open");
                    exit(1);
                }
                // Save current stdout and stderr
                saved_stdout = dup(STDOUT_FILENO);
                saved_stderr = dup(STDERR_FILENO);
                // Redirect stdout and stderr to the file
                if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                    perror("dup2");
                    close(fd);
                    exit(1);
                }
                close(fd); // No need to keep this fd open anymore
            }
        }

        // Execute the command
        if (execv(path, args) == -1) {
            perror("wsh");
            exit(1);  // If execv fails, exit
        }
    } else if (pid < 0) {
        // Fork failed
        perror("wsh: fork");
        return;
    } else {
        // Parent process: wait for the child to complete
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
    if (!from_history) {
        add_to_history(command_str);
    }
    free(path);

    // Restore original stdout, stdin, stderr
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);
        close(saved_stderr);
    }
    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO);
        close(saved_stdin);
    }
}

void execute_builtin_cmd(builtin_cmd_t cmd, char **args, int argc, Redirection *redir){
     int saved_stdout = -1;
     int saved_stderr = -1;
     int saved_stdin = -1;

    if (redir->type != REDIR_NONE) {
        int fd;
        //open file based on redir->type
        if (redir->type == REDIR_OUTPUT || redir->type == REDIR_OUTPUT_APPEND) {
            fd = open(redir->file, O_WRONLY | O_CREAT | (redir->type == REDIR_OUTPUT_APPEND ? O_APPEND : O_TRUNC), 0644);
            if (fd < 0) {
                perror("open");
                return;
            }
            saved_stdout = dup(STDOUT_FILENO);  //save current stdout

            // Redirect stdout or the file descriptor provided by redir->fd
            if (dup2(fd, redir->fd) < 0) {  
                perror("dup2");
                close(fd);
                return;
            }
            close(fd);
        } else if (redir->type == REDIR_INPUT) {
            fd = open(redir->file, O_RDONLY);
            if (fd < 0) {
                perror("open");
                return;
            }
            saved_stdin = dup(STDIN_FILENO);  //save current stdin
            // Redirect stdin or the file descriptor provided by redir->fd
            if (dup2(fd, redir->fd) < 0) {  
                perror("dup2");
                close(fd);
                return;
            }
            close(fd);
        } else if (redir->type == REDIR_OUTPUT_ERROR || redir->type == REDIR_OUTPUT_ERROR_APPEND) {
            fd = open(redir->file, O_WRONLY | O_CREAT | (redir->type == REDIR_OUTPUT_ERROR_APPEND ? O_APPEND : O_TRUNC), 0644);
            if (fd < 0) {
                perror("open");
                return;
            }
            saved_stdout = dup(STDOUT_FILENO);
            saved_stderr = dup(STDERR_FILENO);
            //redirect stdout and stderr or the file descriptor provided by redir->fd
            if (dup2(fd, redir->fd) < 0 || dup2(fd, STDERR_FILENO) < 0) {
                perror("dup2");
                close(fd);
                return;
            }
            close(fd);
        }
    }

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

    // Restore original stdout, stdin, and stderr if they were redirected
    if (saved_stdout >= 0) {
        dup2(saved_stdout, STDOUT_FILENO);  
        close(saved_stdout);
    }
    if (saved_stderr >= 0) {
        dup2(saved_stderr, STDERR_FILENO);  
        close(saved_stderr);
    }
    if (saved_stdin >= 0) {
        dup2(saved_stdin, STDIN_FILENO); 
        close(saved_stdin);
    }
    return;
}

void run_loop(FILE *input_stream){
    char *line;
    char *command_str_copy;
    char **parsed_command;
    int argc;
    Redirection redir;

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

        parsed_command = parse_line(trimmed_line, &argc, &redir);
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
            execute_external_cmd(parsed_command,command_str_copy, 0, &redir);
            free(command_str_copy);
        }else{
            free(command_str_copy);
            execute_builtin_cmd(command, parsed_command, argc, &redir);
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

    init_history();

    run_loop(input_stream); //main program loop

    if(input_stream != stdin){
        fclose(input_stream);
    }
    return 0;
}