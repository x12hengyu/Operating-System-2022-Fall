#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include <errno.h> 
#include <sys/wait.h>
#include <fcntl.h>


#define DEFAULT_PROMT "wish> "
#define DEFAULT_TOKEN_LIST_SIZE 512

char *default_delim = " \t\r\n\a";

char *PATH[256];
int exit_val = -1;
int found_redir_if = -1;

/*
  main methods:
*/
void error_msg();
void main_loop();
void batch_loop(char ** argv);
int process_line(char *line);
char *read_line();
char **parse_line(char *line, char *delim);
int start_process(char **args, char *redir);
int execute(char **args, char *redir);
/*
  built-in shell commands declarations:
 */
int wish_cd(char **args);
int wish_path(char **args);
int wish_exit(char **args);
int wish_if(char **args);

void error_msg() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

int wish_cd(char **args)
{
    if (args[1] == NULL || args[2] != NULL) {
        error_msg();
    } else {
        // change directory
        if (chdir(args[1]) != 0) {
            error_msg(); // failed to change directory
        }
    }
    return 1;
}

int wish_path(char **args)
{
    // no arguments, clear PATH, disable shell from any command except build-in
    if (args[1] == NULL) {
        for(int j = 0; j < 256 && PATH[j] != NULL; j++) {
            PATH[j] = NULL;
        }
        return 1;
    }
    // overwrites new path arguments
    int i = 0;
    while(args[i+1] != NULL) {
        PATH[i] = malloc(256);
        strcpy(PATH[i], args[i+1]);
        i++;
    } 
        
    return 1;
}

int wish_exit(char **args)
{
    // it's an error to pass any arguments 
    if (args[1] != NULL) {
        error_msg();
    }
    exit(0);
}

int wish_if(char **args)
{   
    char **args_for_cmd = malloc( 3 * sizeof(char*) );
    char **args_after_then = malloc( 32 * sizeof(char*) );
    int pos_cmp = -1;
    int pos_then = -1;
    int pos_val = -1;

    // edge case test
    int i = 0;
    int cnt_if = 0, cnt_then = 0, cnt_fi = 0;
    while(args[i] != NULL) {
        if (strcmp(args[i], "if") == 0) {
            cnt_if++;
        }
        if (strcmp(args[i], "then") == 0) {
            cnt_then++;
        }
        if (strcmp(args[i], "fi") == 0) {
            cnt_fi++;
        }
        i++;
    }
    if (cnt_if != cnt_then || cnt_then != cnt_fi || cnt_fi != cnt_if) {
        error_msg();
        return 1;
    }

    // start process
    i = 1;
    while(args[i] != NULL && strcmp(args[i], "then") != 0) {
        if (strcmp(args[i], "==") == 0 || strcmp(args[i], "!=") == 0) {
            pos_cmp = i;
            // put cmds to args
            int j = 1;
            while (j < pos_cmp) {
                args_for_cmd[j-1] = strdup(args[j]);
                j++;
            }
        }
        i++;
    }

    if (args[i] != NULL && strcmp(args[i], "then") == 0) {
        pos_then = i;
        pos_val = i-1;
    }
    i++;

    // edge case test
    if (pos_then == -1 || pos_cmp == -1 || pos_cmp <= 1) {
        error_msg();
        return 1;
    }

    // edge case test
    if (strcmp(args[i], "fi") == 0) {
        return 1;
    }
    // printf("line 118: %s, %s, %s\n", args_for_cmd[0], args_for_cmd[1], args_for_cmd[2]);
    // run cmd
    start_process(args_for_cmd, NULL);
    
    // compare
    int val = atoi(args[pos_val]);
    // printf("line 124: %d\n", exit_val);
    if ((strcmp(args[pos_cmp], "==") == 0 && exit_val == val) || (strcmp(args[pos_cmp], "!=") == 0 && exit_val != val)) {
        int j = pos_then + 1;
        int found_fi = -1;
        
        // get commands
        while(args[j] != NULL) {
            // if one == 1 then     if one != 0 then hello fi     fi (null)
            // printf("loop (%d): %s, %s\n", j-pos_then-1, args[j], args[j+1]);
            if (args[j+1] == NULL && strcmp(args[j], "fi") == 0) {
                found_fi = j;
                break;
            }
            args_after_then[j-pos_then-1] = strdup(args[j]);
            j++;
        }
        // printf("%d\n", j-pos_then-1);
        args_after_then[j-pos_then-1] = NULL;
        // printf("line 139: %s, %s, %s\n", args_after_then[0], args_after_then[1], args_after_then[2]);
        // printf("line 142: %d, %d\n", found_fi, pos_then);
        if (found_fi - pos_then == 1) {
            return 1;
        }

        if (found_fi != -1) {
            //if (found_fi - pos_then > 1) {
                // printf("run this 145\n");
                execute(args_after_then, NULL);
            //}
        } else {
            // printf("no fi error\n");
            error_msg();
        }
        
        
        free(args_for_cmd);
        free(args_after_then);
    }
    return 1;
}

char *read_line() 
{
    char *buffer; // store line
    size_t buffer_size = 0;

    if (getline(&buffer, &buffer_size, stdin) == -1) {
        int line_end = feof(stdin);
        if (line_end) {
            exit(1); // reach EOF
        } else {
            error_msg();
            exit(0);
        }
    }
    return buffer;
}

/*
parse_line does both parse line and parallel commands
*/
char **parse_line(char *line, char *delim)
{   
    int buffer_size = DEFAULT_TOKEN_LIST_SIZE;
    char **tokens = malloc(buffer_size * sizeof(char*));
    int pos = 0;

    if (!tokens) {
        error_msg();
        exit(0);
    }

    // citation: https://c-for-dummies.com/blog/?p=1769
    char *token;
    while((token = strsep(&line, delim)) != NULL) {
        if (*token == '\0') continue; // this line took me whole night to figure out
        tokens[pos] = token;
        pos++;
    }
    tokens[pos] = NULL;
    return tokens;
}

int find_path(char *arg, char *temp) {
    int i = 0;
    // determine the correct path
    while(PATH[i] != NULL) {
        strcpy(temp, PATH[i]);

        if (temp[strlen(temp) - 1] != '/' && arg[0] != '/') {
            strcat(temp, "/");
        }
        strcat(temp, arg);
        
        // find instruction
        if (access(temp, X_OK) == 0) {
            return 1;
            break;
        }
        i++; // find other path next
    }
    return 0;
}

int start_process(char **args, char *redir) {
    int status;

    char temp[128];
    int found = find_path(args[0], temp);
    if (!found) {
        error_msg();
        return 1;
    }
    
    pid_t pid, wait_pid;
    pid = fork();

    switch(pid) {
        case 0:
            // child process execute
            // redirection, find file
            if (redir != NULL) {
                int fd = open(redir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                
                if (fd > 0) {
                    dup2(fd, fileno(stdout));
                    fflush(stdout);
                } else {
                    // exit
                    error_msg();
                }
            }
            execv(temp, args); // execute no matter execute

        case -1:
            // printf("error forking\n");
            error_msg(); // Error forking
        default:
            // no error
            exit_val = -1;
            wait_pid = waitpid(pid, &status, 0);
            // printf("status %d \n",  WEXITSTATUS(status));
            exit_val = WEXITSTATUS(status);
    }

    return 1;
}

int execute(char **args, char *redir)
{
    if (args[0] == NULL) {
        return 1; // An empty command was entered.
    }

    if (strcmp(args[0], "cd") == 0) {
        return wish_cd(args);
    }
    if (strcmp(args[0], "path") == 0) {
        return wish_path(args);
    }
    if (strcmp(args[0], "exit") == 0) {
        return wish_exit(args);
    }
    if (strcmp(args[0], "if") == 0) {
        return wish_if(args);
    }

    return start_process(args, redir);
}

int process_line(char *line) {
    int status;
    
    char **redirs_tokens;
    char **tokens;
    char **redirs;

    char *copystr = malloc(strlen(line));
    strcpy(copystr, line);

    // redirection
    redirs_tokens = parse_line(line, ">"); // [0]: commands [1]: file
    // parse commands and file name
    tokens = parse_line(redirs_tokens[0], default_delim);
    redirs = parse_line(redirs_tokens[1], default_delim);

    // execute
    if (strchr(copystr, '>') != NULL) {
        // count '>'
        int i, count;
        for (i = 0, count = 0; copystr[i]; i++)
            count += (copystr[i] == '>');
        // redirection errors
        if (count > 1 || redirs[0] == NULL || redirs[1] != NULL) {
            error_msg();
            status = 1;
        } else {
            status = execute(tokens, redirs[0]); // correct redirect
        }
    } else {
        status = execute(tokens, NULL);
    }
    
    free(redirs_tokens);
    free(redirs);
    free(tokens);
    free(copystr);

    return status;
}

void batch_loop(char ** argv)
{
    FILE *file;

    if (!(file = fopen(argv[1], "r"))){
        error_msg();
        exit(1);
    }

    char *line;
    size_t len = 0;
    ssize_t read;

    int status = 1;
    // read file line by line
    while (status && (read = getline(&line, &len, file)) != -1) {
        status = process_line(line);
    }
    free(line);
}

void main_loop() 
{
    int status;
    char *line;
    
    do {
        printf(DEFAULT_PROMT);
        // printf("%s, %s, %s:", PATH[0], PATH[1], PATH[2]);
        line = read_line();
        status = process_line(line);

        free(line);

    } while (status);
}

int main(int argc, char ** argv) 
{   
    PATH[0] = "/bin"; // default path
    // printf("%d\n", argc);
    // switch model
    if (argc == 2) {
        batch_loop(argv);
    } else if (argc == 1) {
        main_loop();
    } else if (argc < 1 || argc > 2) {
        error_msg(); // wrong arguments
        return 1;
    }

    // for (int i = 0; i < 256; i++) {
    //     free(PATH[i]);
    // }

    return 0;
}