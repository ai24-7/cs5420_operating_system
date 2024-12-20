
/*
 * Ostermann's shell header file
 */

#define maximum_arguments 500

enum redirectionType
{
    REDIR_INPUT = 1,
    REDIR_OUTPUT = 2,
    REDIR_OUTPUT_APPEND = 3,
    REDIR_ERROR = 4,
    REDIR_ERROR_APPEND = 5
};

/* This is the definition of a command */
struct command {
    char *outfile;
    char *errorLogFile;
    char *infile;

    char *command;
    int argc;
    char *argv[maximum_arguments];

    char and_flag;  // For && operator
    char or_flag;   // For || operator
    int lastStatus; 
    int line_number;
    char var_flag;
    char var_exp_flag;
    char output_append;
    char error_append;
    
    struct command *nextCommand;
};

/* externals */
extern int yydebug;
extern int debugMode;
extern int synerror;
extern int lines; 


/* you should use THIS routine instead of malloc */
void *MallocZ(int numbytes);

/* global routine decls */
void doline(struct command *pcmod);
void handleLine(struct command *pcmod);
void echo(struct command *pcmod);

void __putenv(char *envValue);
void execChangeDirectory(struct command *pcmod);
void _putenv(struct command *pcmod);


int yyparse(void);

struct redirs *PrepareRedir(char *filename, int redirToken);
char *removeQuots(char *text);
