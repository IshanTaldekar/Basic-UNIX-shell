 /*
  CSE 4733 - OPERATING SYSTEMS: Programming Assignment 2

  * Author:
    Name: Ishan Taldekar
    Net-id: iut1
    Student-id: 903212069

  * Description:
    An implementation of a basic UNIX shell that can process and execute commands parsed in from the terminal. This mock shell supports
    pipes as well as input/output redirection. It supports a large subset of UNIX shell commands through the execvp call. Some calls that
    aren't supported by execvp are implemented using built-in functions. The terminal also handles keyboard interrupts without terminating.
    The program can be killed using Cntl+D.
    
  */


/* GLOBALS */
int *background_process_ids;  // shared virtual memory to hold background process ids which are otherwise unavailable.
extern char **environ;  // environment variables
char original_working_directory[MAX_PATH_LENGTH];
char current_working_directory[MAX_PATH_LENGTH];
char *home_directory;
int background_flag = 0;  // identifies a background process (set when '&' character encountered)
char *background_jobs_names[200];   // !! CHECK !!
int background_counter = 0;  // number of background processes
int error_flag = 0;  // identifies syntax errors within commands
char background_string[MAX_INPUT_LENGTH];  // most recent command with & character !! CHECK !!

/*
 REFERENCES:

 The following implementation of a UNIX shell is based upon information from the following sources:
 1. https://www.geeksforgeeks.org/making-linux-shell-c/
 1. https://indradhanush.github.io/blog/writing-a-unix-shell-part-1/
 2. https://indradhanush.github.io/blog/writing-a-unix-shell-part-2/
 3. Advanced Programming in the UNIX Environment, 3rd Edition
 4. Linux System Programming: Talking Directly To The Kernel And C Library Second Edition
 5. https://www.geeksforgeeks.org/making-linux-shell-c/
 6. Linux man pages.
*/


void shell_history();  // function declaration

/* handles Cntl-C press by doing nothing */
void signal_interrupt_handler(int signal_number) { NULL; }

/* handles Cntl-D press by writing history to a history log-file */
void signal_quit_handler(int signal_number) {

    int hist_fd = open("histlog", O_WRONLY | O_TRUNC | O_CREAT, 0666);  // opens a log file to write the commands history
    int hist_err_fd = open("/dev/null", O_WRONLY | O_TRUNC | O_CREAT, 0666);

    dup2(hist_err_fd, 2);
    dup2(hist_fd, 1);  // sets the output file descriptor to the log file created above

    close(hist_fd);
    shell_history();  // shell history outputs command history which is now written to log file.
}

/* converts pid to an array so that it can be easily stored in the shared memory (which is an int array) */
void convert_pid_to_array(int number, int *pid_array) { 

    for (int i = 4; i >= 0; i--) {
        pid_array[i] = number % 10;
        number = number / 10;
    }

}

/* converts a 5 element array holding the 5 digits of a process identifier into a single integer */
int convert_pid_array_to_int(int *pid_array) {
    
    int i, k = 0;

    for (i = 0; i < 5; i++) {
        k = 10 * k + pid_array[i];
    }

    return k;
}

/* saves process identifiers of the commands that are to be executed in the background */
void save_background_pid(int pid) {

    int pid_array[5];
    convert_pid_to_array(pid, pid_array);

    int counter = 0;
    for (int i = (background_counter) * 5; i < (background_counter + 1) * 5; i++) {
        background_process_ids[i] = pid_array[counter];
        counter++;
    }

}

/* prompts the user for input and catches Cntl-D to terminate shell */
int prompt_user_input(char *parsed_string) {

    char input_buffer[MAX_INPUT_LENGTH];
    printf("\n%% ");

    // COMMENTED OUT(alternative): input_buffer = readline("\n% ");

    char *status = fgets(input_buffer, MAX_INPUT_LENGTH, stdin);

    if (status == NULL) {
        raise(SIGUSR1);
        execlp("killall", "-9", "shell", NULL);
    }

    if (strlen(input_buffer) != 0) {

        if (strcmp(input_buffer, "history") != 0) {
            
            /*
            place string at the end of the history list. the associated data field (if any) is set to NULL.
            Source: https://linux.die.net/man/3/history
            */
            add_history(input_buffer);

        }

        strcpy(parsed_string, input_buffer);
        return 0;

    }

    return 1;

}

/* uses strtok() call to separate user input string into two strings at the pipe ('|') */
int tokenize_by_pipe_delimiter(char *parsed_string, char **tokenized_string) {
    int pipe_found = 0;

    /* looking for a pipe operator for error detection */
    for (int i = 0; parsed_string[i]; i++) {
        if (parsed_string[i] == '|') {
            pipe_found = 1;
        }
    }
    if (!parsed_string[0]) {
        return 0;  // No pipe to be found
    }

    for (int i = 0; i < 2; i++) {
        if (i == 0) {
            tokenized_string[i] = strtok(parsed_string, "|");
            if (tokenized_string[i] == NULL) {
                break;
            }
        } else {
            tokenized_string[i] = strtok(NULL, "|");
        }
    }

    /* if there is a pipe operator but the second command is missing, then raise an error */
    if (tokenized_string[1] == NULL && pipe_found) {
        error_flag = 1;
    }

    if (tokenized_string[1] == NULL) {
        return 0;
    } else {
        return 1;
    }
}

/* tokenize strings along new line and space characters, and extract comments from user input */
void tokenize_by_space_delimiter(char *parsed_string, char **tokenized_string, char **comments) {

    char *tokens[MAX_TOKEN_COUNT];
    int comments_counter = 0;

    tokens[0] = strtok(parsed_string, " \n");

    int counter = 1;
    while (counter < MAX_TOKEN_COUNT && (tokens[counter] = strtok(NULL, " \n")) != NULL) {
        if (strlen(tokens[counter])) counter++;
    }

    background_flag = 0;
    counter = 0;

    for (int i = 0; tokens[i]; i++) {
    
        if (strcmp(tokens[i], "&") == 0) {
            background_flag = 1;
            continue;
        }

        if (tokens[i][0] == '/' && tokens[i][1] == '*' && strcmp(tokens[i], "/*")) {
            comments[comments_counter] = tokens[i];
            comments_counter++;
            continue;
        }

        if (strcmp(tokens[i], "/*") == 0) {
            /*
            It was getting unnecessarily difficult to verify that comment blocks were complete.
            Instead, this segment performs under the assumption that comments only appear at the end of a command.
            After a comment block is opened, all input following '/*' is discarded.
            */

            while (tokens[i]) {
                comments[comments_counter] = tokens[i];
                i++;
                comments_counter++;
            }
            continue;

        }

        tokenized_string[counter] = tokens[i];
        counter++;

    }

    tokenized_string[counter] = NULL;

}

/* built in function to execute the 'cd' command */
void change_directory(char **parsed_string) {

    int argument_counter = 0;

    while (parsed_string[argument_counter] != NULL) {

        argument_counter++;

        if (argument_counter > 2) {
            fprintf(stderr, "ERROR: cd - two many arguments given...\n");
            return;
        }

    }

    if (parsed_string[1] != NULL) {

        if (chdir(parsed_string[1]) == 0) {
            getcwd(current_working_directory, MAX_PATH_LENGTH);
            setenv("PWD", current_working_directory, 1);
            return;
        } else {
            perror("ERROR: ");
            return;
        }

    } else {

        if (chdir(home_directory) == 0) {
            getcwd(current_working_directory, MAX_PATH_LENGTH);
            setenv("PWD", current_working_directory, 1);
            return;
        } else {
            perror("ERROR: ");
            return;
        }

    }

}

/*
displays the jobs running in the background and determines their status using process ids stored in a shared memory.
function for execution of the built in 'jobs' command.
*/
void display_running_background_jobs() {

    int first_pid = 0;
    int pid_array[5];
    int counter = 0, n = 1;

    for (int i = 0; i < SHARED_MEM_SIZE; i++) {

        if (!background_jobs_names[n - 1]) break;

        if (counter < 5) {

            pid_array[counter] = background_process_ids[i];
            counter++;

        } else {

            int pid = convert_pid_array_to_int(pid_array);

            if (i == 5) {
                first_pid = pid;
            } else {
                if (first_pid == pid) return;
            }

            if (kill(pid, 0) < 0) {
                if (errno == ESRCH) printf("[%d] Stopped\t\t%s", n, background_jobs_names[n - 1]);
            } else {
                printf("[%d] Running\t\t%s", n, background_jobs_names[n - 1]);
            }

            n++;
            counter = 1;
            pid_array[0] = background_process_ids[i];

        }

    }

}

/*
prints environment information to screen.
function for execution of the built in 'env' command.
*/
void show_environment() {

    int counter = 0;

    while (environ[counter] != NULL) {
        printf("%s\n", environ[counter]);
        counter++;
    }

}

/*
prints the elements of the history list to standard output.
function for execution of the built in 'history' command.
Reference: https://tiswww.case.edu/php/chet/readline/history.html
*/
void shell_history() {

    struct _hist_state *history_state;
    history_state = history_get_history_state();

    register HIST_ENTRY **current_history_list;
    current_history_list = history_list();

    if (current_history_list) {

        int start_at = 0;
        int num = 1;

        if (history_state->length > 10) {
            start_at = history_state->length - 10;
        }

        for (int i = start_at; i < history_state->length; i++) {
            printf("[%d]\t%s", num, current_history_list[i]->line);
            num++;
        }

    }

}

/*
traverses the input string tokens in search of IO redirection commands.
returns 1 if found, returns 0 otherwise.
*/
int lookup_IO_redirection_commands(char **parsed_string) {

    int counter = 0;
    int redirection_command_found_flag = 0;

    while (parsed_string[counter] != NULL) {

        if (strcmp(parsed_string[counter], "<") == 0 || strcmp(parsed_string[counter], ">") == 0 || \
        strcmp(parsed_string[counter], ">>") == 0) {
            return 1;
        }

        counter++;

    }

    return 0;

}

/*
traverses the input string tokens in search of the required built-in commands: cd, history, env, jobs.
returns 1 if found, 0 otherwise.
*/
int lookup_built_in_commands(char **parsed_string) {

    int i;
    char *built_in_commands[4] = {"cd", "history", "env", "jobs"};

    for (i = 0; i < 4; i++) {

        if (strcmp(built_in_commands[i], parsed_string[0]) == 0) {
            break;
        }

    }

    switch (i) {

        case 0:
            change_directory(parsed_string);
            return 1;

        case 1:
            shell_history();
            return 1;

        case 2:
            show_environment();
            return 1;

        case 3:
            display_running_background_jobs();
            return 1;

        default:
            return 0;

    }

}

/*
traverses the input string to characterize, tokenize and clean it.
returns a 'command type' code -
    1 - if its a regular command.
    2 - if its a simple piped command.
    3 - if its a regular command with IO redirection.
    4 - if its a piped command with IO redirection.
*/
int process_input_string(char *parsed_string, char **tokenized_string, char **tokenized_piped_string, char **comments) {

    char *parsed_piped_string[2];
    int pipe_found_flag;

    if ((pipe_found_flag = tokenize_by_pipe_delimiter(parsed_string, parsed_piped_string))) {
        tokenize_by_space_delimiter(parsed_piped_string[0], tokenized_string, comments);
        tokenize_by_space_delimiter(parsed_piped_string[1], tokenized_piped_string, comments);
    } else {
        tokenize_by_space_delimiter(parsed_string, tokenized_string, comments);
    }

    if (lookup_built_in_commands(tokenized_string)) return -1;

    if (pipe_found_flag) {

        if (lookup_IO_redirection_commands(tokenized_string) ||
            lookup_IO_redirection_commands(tokenized_piped_string)) {
            return 4;
        }

        return 2;

    } else {

        if (lookup_IO_redirection_commands(tokenized_string)) {
            return 3;
        }

        return 1;

    }

}

/* Function to execute simple commands without IO redirection or piping */
void execute_regular_commands(char **parsed_string) {
    pid_t pid;

    if (error_flag) {

        fprintf(stderr, "ERROR: unexpected tokens around |\n");
        error_flag = 0;
        return;

    }

    if ((pid = fork()) < 0) {

        fprintf(stderr, "ERROR: fork() unsuccessful...");
        return;

    } else {

        if (pid == 0) {

            if (background_flag) {
                int pid = getpid();
                save_background_pid(pid);
            }

            if (execvp(parsed_string[0], parsed_string) < 0) {
                fprintf(stderr, "ERROR: execvp command unsuccessful...\n");
                exit(0);
            }

        } else {

            wait(NULL);
            return;

        }

    }
   
}

/* Function to execute commands that have a simple pipe */
void execute_piped_commands(char **parsed_string, char **parsed_piped_string) {

    if (parsed_string[0] == NULL || parsed_piped_string[0] == NULL) {
        fprintf(stderr, "ERROR: unexpected token(s) around |\n");
        return;
    }

    /* Reference: https://linux.die.net/man/2/pipe */
    int pipefd[2];
    pid_t pid1, pid2;

    if (pipe(pipefd) < 0) {
        fprintf(stderr, "ERROR: pipe failure");
        exit(1);
    }

    if ((pid1 = fork()) < 0) {
        fprintf(stderr, "ERROR: fork() unsuccessful...");
        return;
    }

    if (pid1 == 0) {

        /* Close the read end since it isn't required. Only write end will be used by the first command */
        close(pipefd[0]);
        dup2(pipefd[1], 1); // makes newfd be the copy of oldfd, closing newfd first if necessary.

        if (background_flag) {
            int pid = getpid();
            save_background_pid(pid);
        }

        if (execvp(parsed_string[0], parsed_string) < 0) {
            fprintf(stderr, "ERROR: execvp command could not execute command 1...\n");
        }

    } else {

        if ((pid2 = fork()) < 0) {
            fprintf(stderr, "ERROR: fork() unsuccessful...");
        }

        if (pid2 == 0) {

            /* Close the write end since it isn't required. Only read end will be used by the second command */
            close(pipefd[1]);
            dup2(pipefd[0], 0);

            if (background_flag) {
                int pid = getpid();
                save_background_pid(pid);
            }

            if (execvp(parsed_piped_string[0], parsed_piped_string) < 0) {
                fprintf(stderr, "ERROR: execvp command could not execute command 2...\n");
            }

        }

    }

    waitpid(pid1, NULL, 0);
    close(pipefd[1]);
    waitpid(pid2, NULL, 0);

}

/* executes simple commands with IO redirection */
void execute_redirected_command(char **parsed_string) {

    char *redirected_commands[MAX_TOKEN_COUNT];
    int stdin_fd, stdout_fd, stderr_fd;
    int new_instream_fd, new_outstream_fd, new_errstream_fd;
    char *input_file, *output_file, *error_file;

    int counter = 0;

    stdin_fd = dup(0);
    stdout_fd = dup(1);
    stderr_fd = dup(2);

    for (int i = 0; parsed_string[i]; i++) {

        if (strcmp(parsed_string[i], "<") == 0) {

            if ((input_file = parsed_string[i + 1]) == NULL) {
                fprintf(stderr, "ERROR: unexpected token after <\n");
                return;
            }

            if ((new_instream_fd = open(input_file, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                fprintf(stderr, "ERROR: failed to open file %s", input_file);
            }

            dup2(new_instream_fd, 0);
            close(new_instream_fd);
            i++;
            continue;

        }

        if (strcmp(parsed_string[i], ">>") == 0) {

            if ((output_file = parsed_string[i + 1]) == NULL) {
                fprintf(stderr, "ERROR: unexpected token after >>\n");
                return;
            }

            new_outstream_fd = open(output_file, O_APPEND | O_WRONLY | O_CREAT, 0666);
            dup2(new_outstream_fd, 1);
            close(new_outstream_fd);
            i++;
            continue;

        }

        if (strcmp(parsed_string[i], ">") == 0) {

            if ((output_file = parsed_string[i + 1]) == NULL) {
                fprintf(stderr, "ERROR: unexpected token after >\n");
                return;
            }

            new_outstream_fd = creat(output_file, 0644);
            dup2(new_outstream_fd, 1);
            close(new_outstream_fd);
            i++;
            continue;

        }

        if (strcmp(parsed_string[i], "2") == 0) {

            if (parsed_string[i + 1] == NULL) {
                fprintf(stderr, "ERROR: unexpected token after 2..\n");
                return;
            }

            if ((error_file = parsed_string[i + 2]) == NULL) {
                fprintf(stderr, "ERROR: unexpected token after <\n");
                return;
            }

            new_errstream_fd = creat(error_file, 0644);
            dup2(new_errstream_fd, 2);
            close(new_errstream_fd);
            i = i + 2;
            continue;

        }

        redirected_commands[counter] = parsed_string[i];
        counter++;

    }

    redirected_commands[counter] = NULL;
    pid_t pid;

    switch (pid = fork()) {

        case 0:

            if (background_flag) {
                int pid = getpid();
                save_background_pid(pid);
            }

            execvp(redirected_commands[0], redirected_commands);

            // If execvp is successful, then the remaining two lines of code below will not execute.
            fprintf(stderr, "ERROR: execvp command could not be executed...\n");
            break;

        case -1:  // This code snippet runs if the fork fails for some reason.
            fprintf(stderr, "ERROR: fork() unsuccessful...");
            exit(1);

        default:
            // This piece of code is run only by the parent.
            waitpid(pid, NULL, 0);
            dup2(stdin_fd, 0);
            dup2(stdout_fd, 1);
            dup2(stderr_fd, 2);

            close(stdin_fd);
            close(stdout_fd);
            close(stderr_fd);
            break;

    }

}

/* Function to execute piped commands with IO redirection */
void execute_redirected_piped_commands(char **parsed_string, char **parsed_piped_string) {

    char *redirected_commands[MAX_TOKEN_COUNT];
    char *redirected_piped_commands[MAX_TOKEN_COUNT];

    char *input_file, *output_file, *error_file;
    int new_instream_fd, new_outstream_fd, new_errstream_fd;
    int counter = 0;

    if (parsed_string[0] == NULL || parsed_piped_string[0] == NULL) {
        fprintf(stderr, "ERROR: unexpected token(s) around |\n");
        return;
    }

    /* Reference: https://linux.die.net/man/2/pipe */
    int pipefd[2];
    pid_t pid1, pid2;

    if (pipe(pipefd) < 0) {
        fprintf(stderr, "ERROR: pipe failure");
        exit(1);
    }

    if ((pid1 = fork()) < 0) {
        fprintf(stderr, "ERROR: fork() unsuccessful...");
        return;
    }

    if (pid1 == 0) {

        close(pipefd[0]);  // Close the read end since it isn't required. Only write end will be used.
        dup2(pipefd[1], 1); // makes newfd be the copy of oldfd, closing newfd first if necessary.

        if (lookup_IO_redirection_commands(parsed_string)) {

            for (int i = 0; parsed_string[i]; i++) {

                if (strcmp(parsed_string[i], "<") == 0) {

                    if ((input_file = parsed_string[i + 1]) == NULL) {
                        fprintf(stderr, "ERROR: unexpected token after <\n");
                        return;
                    }

                    if ((new_instream_fd = open(input_file, O_RDONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0) {
                        fprintf(stderr, "ERROR: failed to open file %s", input_file);
                    }

                    dup2(new_instream_fd, 0);
                    close(new_instream_fd);
                    i++;

                }

                if (strcmp(parsed_string[i], "2") == 0) {

                    if ((error_file = parsed_string[i + 2]) == NULL) {
                        fprintf(stderr, "ERROR: unexpected token after <\n");
                        return;
                    }

                    new_errstream_fd = creat(error_file, 0644);
                    dup2(new_errstream_fd, 2);
                    close(new_errstream_fd);
                    i = i + 2;
                    continue;

                }

                if (strcmp(parsed_string[i], ">>") == 0 || strcmp(parsed_string[i], ">") == 0) {
                    fprintf(stderr,
                            "ERROR: output redirection cannot be performed in this command due to the pipe...\n");
                    return;
                }

                redirected_commands[counter] = parsed_string[i];
                counter++;

            }

            redirected_commands[counter] = NULL;

            if (background_flag) {
                int pid = getpid();
                save_background_pid(pid);
            }

            execvp(redirected_commands[0], redirected_commands);

        }

        if (execvp(parsed_string[0], parsed_string) < 0) {
            fprintf(stderr, "ERROR: execvp command could not execute command 1...\n");
        }

    } else {

        if ((pid2 = fork()) < 0) {
            fprintf(stderr, "ERROR: fork() unsuccessful...");
        }

        if (pid2 == 0) {
            close(pipefd[1]);  // Close the write end since it isn't required. Only read end will be used.
            dup2(pipefd[0], 0);

            if (lookup_IO_redirection_commands(parsed_piped_string)) {

                for (int i = 0; parsed_piped_string[i]; i++) {

                    if (strcmp(parsed_piped_string[i], "<") == 0) {
                        fprintf(stderr,
                                "ERROR: output redirection cannot be performed in this command due to the pipe...\n");
                        return;
                    }

                    if (strcmp(parsed_piped_string[i], ">>") == 0) {

                        if ((output_file = parsed_piped_string[i + 1]) == NULL) {
                            fprintf(stderr, "ERROR: unexpected token after >>\n");
                            return;
                        }

                        // printf("in append function");
                        new_outstream_fd = open(output_file, O_APPEND | O_WRONLY | O_CREAT, 0666);
                        dup2(new_outstream_fd, 1);
                        close(new_outstream_fd);
                        i++;
                        continue;

                    }

                    if (strcmp(parsed_piped_string[i], ">") == 0) {

                        if ((output_file = parsed_piped_string[i + 1]) == NULL) {
                            fprintf(stderr, "ERROR: unexpected token after >\n");
                            return;
                        }

                        new_outstream_fd = creat(output_file, 0644);
                        dup2(new_outstream_fd, 1);
                        close(new_outstream_fd);
                        i++;
                        continue;

                    }

                    if (strcmp(parsed_piped_string[i], "2") == 0) {

                        if (parsed_piped_string[i + 1] == NULL) {
                            fprintf(stderr, "ERROR: unexpected token after 2..\n");
                            return;
                        }

                        if ((error_file = parsed_piped_string[i + 2]) == NULL) {
                            fprintf(stderr, "ERROR: unexpected token after <\n");
                            return;
                        }

                        new_errstream_fd = creat(error_file, 0644);
                        dup2(new_errstream_fd, 2);
                        close(new_errstream_fd);
                        i = i + 2;
                        continue;

                    }

                    redirected_piped_commands[counter] = parsed_piped_string[i];
                    counter++;

                }

                redirected_piped_commands[counter] = NULL;

                if (background_flag) {
                    int pid = getpid();
                    save_background_pid(pid);
                }

                execvp(redirected_piped_commands[0], redirected_piped_commands);

                /* If execvp is successful, then the remaining two lines of code below will not execute */
                fprintf(stderr, "ERROR: execvp command could not be executed...\n");

            }

            if (background_flag) {
                int pid = getpid();
                save_background_pid(pid);
            }

            if (execvp(parsed_piped_string[0], parsed_piped_string) < 0) {
                fprintf(stderr, "ERROR: execvp command could not execute command 2...\n");
                exit(0);
            }

        }

    }

    waitpid(pid1, NULL, 0);
    close(pipefd[1]);
    waitpid(pid2, NULL, 0);

}

void initialize_shell() {

    clear();
    printf("*****************************************************************\n\n");
    printf("CUSTOM SHELL\n\n");
    printf("*****************************************************************\n\n");

    printf("Author: Ishan Taldekar\n");
    printf("Contact: ishantaldekar@protonmail.com\n");

    home_directory = getenv("HOME");

    getcwd(original_working_directory, MAX_PATH_LENGTH);
    strcat(original_working_directory, "/shell");
    setenv("SHELL", original_working_directory, 1);

    sleep(2);
    clear();

}

int main() {

    char user_input[MAX_INPUT_LENGTH];
    char *parsed_tokens[MAX_TOKEN_COUNT];
    char *parsed_piped_tokens[MAX_TOKEN_COUNT];
    char *comments[MAX_TOKEN_COUNT];
    int command_type, fd;
    pid_t pid = 0;

    /* Reference: https://linux.die.net/man/2/mmap */
    if ((background_process_ids = (int *) mmap(0, SHARED_MEM_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED | MAP_ANONYMOUS,
                                               -1, 0)) == MAP_FAILED) {
        fprintf(stderr, "ERROR: MAP failed..\n");
        exit(1);
    }

    signal(SIGUSR1, signal_quit_handler);
    signal(SIGINT, signal_interrupt_handler);
    initialize_shell();

    while (!feof(stdin)) {

        if (prompt_user_input(user_input))
        {
            fprintf(stderr, "ERROR: input stream error...\n");
            continue;
        }

        command_type = process_input_string(user_input, parsed_tokens, parsed_piped_tokens, comments);

        /*
        forking to allow commands to be executed in the background. Since, the parent won't wait for its children's
        children to finish anyways, and the parent is not going to wait for the child to finish if the & operator is
        encountered in the user input ~ the command would be executed as if in the background.
        */
        if ((pid = fork()) < 0) {
            fprintf(stderr, "ERROR: fork system call failed...\n");
            exit(0);
        }

        if (!pid) {

            /* redirecting any output from standard output to mimic the background processes */
            if (background_flag) {
                fd = open("/dev/null", O_CREAT | O_WRONLY | O_TRUNC);
                dup2(fd, 1);
            }

            switch (command_type) {

                case 1:
                    execute_regular_commands(parsed_tokens);
                    break;

                case 2:
                    execute_piped_commands(parsed_tokens, parsed_piped_tokens);
                    break;

                case 3:
                    execute_redirected_command(parsed_tokens);
                    break;

                case 4:
                    execute_redirected_piped_commands(parsed_tokens, parsed_piped_tokens);
                case -1:
                    break;

                default:
                    fprintf(stderr, "ERROR: command was not recognized...\n");

            }

        } else {

            /* only wait for foreground commands, but keep track of the background commands. */
            if (!background_flag) {
                wait(NULL);
            } else {
                background_jobs_names[background_counter] = (char *) malloc(MAX_INPUT_LENGTH);
                strcpy(background_jobs_names[background_counter], background_string);
                background_counter++;
            }

        }

    }

}