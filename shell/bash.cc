
/*
 * Headstart for Ostermann's shell project
 *
 * Shawn Ostermann -- Sept 11, 2022
 * Daniel Safavi -- Nov 1, 2024
 *
 */
 

#include <string.h>
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>


using namespace std;

// types and definitions live in "C land" and must be flagged
extern "C"
{
#include "parser.tab.h"
#include "bash.h"
    extern "C" void yyset_debug(int d);
}



void searchRunCommand(struct command *pcmd, int pipeInFd = -1);
void execAbsCommand(struct command *pcmod, int pipeInFd = -1);
int checkPathExec(string accessPath, struct command *pcmd, bool includePath, int pipeInFd = -1);
string expandEnvVar(char *argVal, bool ignoreQuote = false);

// global debugging flag
int debugMode = 0;

int main(
    int argc,
    char *argv[])
{
    if (debugMode)
        yydebug = 1; /* turn on ugly YACC debugging */

    /* parse the input file */
    yyparse();

    exit(0);
}


void execChangeDirectory(struct command *pcmd)
{
    if (pcmd->argc > 2)
    {
        printf("'cd' requires exactly one argument\n");

        return;
    }

    string pwdEnv = "";

    if (pcmd->argc == 1)
        pwdEnv += getenv("HOME");


    else
        pwdEnv += pcmd->argv[1];


    int err = chdir((char *)pwdEnv.c_str());
    if (err == -1)
    {
        perror(pcmd->argv[1]);
    }
}

void echo(struct command *pcmd)
{
    if (pcmd->argc > 100)
    {
        printf("Error on line %d: Argument list too long\n", pcmd->line_number);
        return;
    }

    int lastArg = (pcmd->argc - 1);
    for (int j = 1; j < pcmd->argc; j++)
    {
        if (pcmd->var_exp_flag == 't')
        {
            printf(j == lastArg ? "%s" : "%s ", expandEnvVar(pcmd->argv[j]).c_str());
        }
        else
        {
            printf(j == lastArg ? "%s" : "%s ", pcmd->argv[j]);
        }
    }
    printf("\n");
}

string expandEnvVar(char *argVal, bool ignoreQuote)
{
    string envStr = "";

    if (argVal[0] == '"' && !ignoreQuote)
    {
        argVal = removeQuots(argVal);
    }

    int k = 0, arg_size = strlen(argVal);
    while (k < arg_size)
    {
        if (argVal[k] == '$')
        {
            char var_str[maximum_arguments];
            int l = 0;
            k = (argVal[k + 1] == '{') ? k + 2 : k + 1;

            while (isalpha(argVal[k]) ||
                   (argVal[k] >= '0' && argVal[k] <= '9') ||
                   (argVal[k] == '_'))
            {
                var_str[l] = argVal[k];
                k++;
                l++;
                if (argVal[k] == '\0')
                    break;
            }
            var_str[l] = '\0';
            k = (argVal[k] == '}') ? k + 1 : k;

            char *envVal = getenv(var_str);

            if (envVal != NULL)
            {
                envStr += envVal;
            }
        }
        if (argVal[k] == '\0')
            break;

        if ((argVal[k] != '$'))
        {
            envStr += argVal[k];
            k++;
        }
    }
    return envStr;
}

void execAbsCommand(struct command *pcmd, int pipeInFd)
{
    int fd_pipe[2], fd_stdin = -1, fd_stdout = -1, fd_stderr = -1;
    int status = 0; // Add this to store command status

    if (pcmd->infile != NULL && strcmp(pcmd->infile, "PIPE") == 0)
    {
        if (pipeInFd != -1)
            fd_stdin = pipeInFd;
    }
    else if (pcmd->infile != NULL)
    {
        strcpy(pcmd->infile, expandEnvVar(pcmd->infile).c_str());
        fd_stdin = open(pcmd->infile, O_RDONLY);

        if (fd_stdin == -1)
        {
            perror(pcmd->infile);
            exit(1); // Exit with error status
        }
    }

    if (pcmd->outfile != NULL && strcmp(pcmd->outfile, "PIPE") == 0)
    {
        if (pipe(fd_pipe) == -1)
        {
            perror("pipe");
            exit(-1);
        }
        else
        {
            fd_stdout = fd_pipe[1];
        }
    }
    else if (pcmd->outfile != NULL)
    {

        strcpy(pcmd->outfile, expandEnvVar(pcmd->outfile).c_str());

        fd_stdout = open(pcmd->outfile, pcmd->output_append == 't' ? (O_APPEND | O_WRONLY) : (O_WRONLY | O_CREAT), 0666);
        if (fd_stdout == -1)
        {
            perror(pcmd->outfile);
            return;
        }
    }

    if (pcmd->errorLogFile != NULL)
    {
        strcpy(pcmd->errorLogFile, expandEnvVar(pcmd->errorLogFile).c_str());

        fd_stderr = open(pcmd->errorLogFile, pcmd->error_append == 't' ? (O_APPEND | O_WRONLY) : (O_WRONLY | O_CREAT), 0666);

        if (fd_stderr == -1)
        {
            perror(pcmd->errorLogFile);
            return;
        }
    }

    int pid = fork();
    if (pid == 0)
    {
        int returnValue = 0;
        while (wait(&returnValue) > 0)
        {
        }

        if (pcmd->infile != NULL)
        {
            dup2(fd_stdin, 0);
            close(fd_stdin);
        }

        if (pcmd->outfile != NULL)
        {
            dup2(fd_stdout, 1);
            close(fd_stdout);
        }

        if (pcmd->errorLogFile != NULL)
        {
            dup2(fd_stderr, 2);
            close(fd_stderr);
        }

        if (pcmd->var_exp_flag == 't')
        {
            for (int i = 1; i < pcmd->argc; i++)
            {
                string var_str_temp = expandEnvVar(pcmd->argv[i]);
                
                if (strlen(var_str_temp.c_str()) > maximum_arguments)
                    pcmd->argv[i] = (char *)var_str_temp.c_str();

                else
                    strcpy(pcmd->argv[i], expandEnvVar(pcmd->argv[i]).c_str());
            }
        }

        execv(pcmd->command, pcmd->argv);
        perror(pcmd->command);

        exit(-1);
    }
    else
    
    {
        waitpid(pid, &status, 0);
        
        pcmd->lastStatus = status;
        // printf("command: %s \nlast status: %d\n", pcmd->command, status);
    }

    if (pcmd->infile != NULL)
        close(fd_stdin);

    if (pcmd->outfile != NULL)
        close(fd_stdout);

    if (pcmd->errorLogFile != NULL)
        close(fd_stderr);

    if (pcmd->outfile != NULL && strcmp(pcmd->outfile, "PIPE") == 0)
        searchRunCommand(pcmd->nextCommand, fd_pipe[0]);

    // Return the command's status through exit
    if (!pcmd->lastStatus)
        exit(1);
}


void searchRunCommand(struct command *pcmd, int pipeInFd)
{
    string accessPath = getenv("PWD");
    checkPathExec(accessPath, pcmd, false, pipeInFd);

    char *_path = strdup(getenv("PATH"));
    char *path = strtok(_path, ":");
    int exist = 0;

    while (path != NULL)
    {
        accessPath = path;

        exist = checkPathExec(accessPath, pcmd, true, pipeInFd);
        if (exist == 0)
            break;

        path = strtok(NULL, ":");
    }

    if (exist == -1)
    {
        printf("%s: command not found\n", pcmd->command);
    }
}

int checkPathExec(string accessPath, struct command *pcmd, bool includePath, int pipeInFd)
{
    int exist = 0;

    if (includePath)
    {
        if (accessPath.back() != '/')
            accessPath += "/";
        accessPath += pcmd->command;
    }
    else
    {
        accessPath += pcmd->command;
    }

    exist = access(accessPath.c_str(), X_OK);

    if (debugMode)
        printf("exist: %d %s\n", exist, accessPath.c_str());

    if (exist != -1)
    {
        pcmd->command = strdup((char *)accessPath.c_str());
        execAbsCommand(pcmd, pipeInFd);
    }
    return exist;
}

void _putenv(struct command *pcmd)
{
    char pathVar[5];
    strncpy(pathVar, pcmd->command, 4);

    if (strcmp(pathVar, "PATH") == 0)
    {
        int count = 0, z = 0;

        while (pcmd->command[z] != '\0')
        {
            if (pcmd->command[z] == ':')
                count++;
            if (count > 100)
            {
                printf("Error on line %d: Path directory list too long\n", pcmd->line_number);
                return;
            }
            z++;
        }
    }

    __putenv(pcmd->command);
}

void __putenv(char *envValue)
{
    if (debugMode)
        printf("putenv:\t%s\n", envValue);

    string envStr = expandEnvVar(envValue);

    int err = putenv(strdup(envStr.c_str()));

    if (err == -1)
        perror("putenv");
}

void doline(struct command *pcmd)
{
    if (synerror == 0)
    {
        printf("========== line %d ==========\n", lines);
        int pipe_cout_input = 0;
        int pipe_cout_output = 0;

        for (struct command *command = pcmd; command; command = command->nextCommand)
        {
            printf("Command name: '%s'\n", command->command);
            for (int q = 0; q < command->argc; q++)
            {
                printf("    argv[%d]: '%s'\n", q, command->argv[q]);
            }
            printf("  stdin:  ");

            if (command->infile == NULL)
            {
                printf("UNDIRECTED\n");
            }

            else
            {
                // pipe order
                if (strcmp(command->infile, "PIPE") == 0)
                {
                    printf("PIPE%d\n", ++pipe_cout_input);
                }
                else
                {
                    printf("'%s'\n", command->infile);
                }
            }

            printf("  stdout: ");
            if (command->outfile == NULL)
            {
                printf("UNDIRECTED");
            }
            else
            {
                if (strcmp(command->outfile, "PIPE") == 0)
                {
                    // pipe order
                    printf("PIPE%d", ++pipe_cout_output);
                }
                else
                {
                    printf("'%s'", command->outfile);
                }
            }
            printf(command->output_append == 't' ? " (append)\n" : "\n");

            printf("  stderr: ");

            command->errorLogFile == NULL ? printf("UNDIRECTED") : printf("'%s'", command->errorLogFile);
            printf(command->error_append == 't' ? " (append)\n" : "\n");
        }
        printf("\n");
    }
}

void handleLine(struct command *pcmd)
{
    // printf("========== line %d ==========\n", lines);
    if (debugMode)
        doline(pcmd);

    if (synerror != 0)
        return;

    char *prompt = getenv("PS1");
    if (prompt != NULL)
    {
        printf("%s", prompt);
        fflush(stdout);
    }

    struct command *currentCommand = pcmd;
    bool skipNext = false;
    int lastStatus = 0;

    while (currentCommand != NULL)
    {
        if (!skipNext)
        {
            if (currentCommand->var_flag == 't')
            {
                _putenv(currentCommand);
                lastStatus = 1;
            }
            else if (strcmp(currentCommand->command, "echo") == 0)
            {
                echo(currentCommand);
                lastStatus = 1;
            }
            else if (strcmp(currentCommand->command, "cd") == 0)
            {
                execChangeDirectory(currentCommand);
                lastStatus = (access(getenv("PWD"), F_OK) == 0);
            }
            else if (strcmp(currentCommand->command, "true") == 0)
            {
                lastStatus = 1;
            }
            else if (strcmp(currentCommand->command, "false") == 0)
            {
                lastStatus = 0;
            }
            else
            {
                pid_t pid = fork();
                if (pid == 0)
                {
                    if (currentCommand->command[0] == '/' || currentCommand->command[0] == '.')
                        execAbsCommand(currentCommand);
                    else
                        searchRunCommand(currentCommand);
                    exit(0);
                }
                else if (pid > 0)
                {
                    int status;
                    waitpid(pid, &status, 0);
                    lastStatus = status;
                }
            }
        }

        if (currentCommand->nextCommand)
        {
            if (currentCommand->and_flag)
            {
                skipNext = !lastStatus;
            }
            else if (currentCommand->or_flag)
            {
                skipNext = lastStatus;
            }
        }

        currentCommand = currentCommand->nextCommand;
    }
}
