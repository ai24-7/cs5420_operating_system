
%{
/*
 * Simple example yacc/bison input file
 *
 * Shawn Ostermann -- Sept 18,2024
 * Daniel Safavi -- Nov 1 ,2024
 *
 * Headstart version
 */

#include <stdio.h>
#include <stdarg.h>
#include "bash.h"
#include <stdlib.h>
#include <string.h>

int yylex();
void yyerror(const char *s,...);
void yywarn(const char *s,...);

struct args {
    char *arg;
    struct args *nextCommand;
    char var_exp_flag;
};

struct redirs {
    int redir_token;
    char *filename;
    struct redirs *nextCommand;
};

#define YYDEBUG 1

int lines = 0;
int synerror = 0;

static int synerrors = 0;
%}

%union {
    char *string;
    struct command *pcmd;

	struct redirs *predir;
    struct args *pargs;

    int number;
}

%token EOLN PIPE AND OR
%token INFILE
%token errorLogFile errorLogFile_APPEND
%token OUTFILE OUTFILE_APPEND
%token <string> WORD VAR_QUOTE VAR_WORD VAR 

%type <pcmd> line cmd pipe_list andor_list and_list 
%type <pargs> optargs arg
%type <predir> optredirs redir

%%
input   : lines
        |
        ;

lines   : oneline
        | oneline lines
        ;

oneline : line eoln
        { if(debugMode) doline($1); handleLine($1); synerror = 0; }
        | eoln
        | error eoln
        ;

eoln    : EOLN
        { ++lines; }
        ;

line    : andor_list
        { $$ = $1; }
        ;

andor_list : and_list
           { $$ = $1; }
           | andor_list OR and_list
           {
               struct command *cmd = $1;
               while (cmd->nextCommand != NULL) cmd = cmd->nextCommand;
               cmd->or_flag = 1;  // Mark for OR operation
               cmd->nextCommand = $3;
               $$ = $1;
           }
           ;

and_list : pipe_list
         { $$ = $1; }
         | and_list AND pipe_list
         {
             struct command *cmd = $1;
             while (cmd->nextCommand != NULL) cmd = cmd->nextCommand;
             cmd->and_flag = 1;  // Mark for AND operation
             cmd->nextCommand = $3;
             $$ = $1;
         }
         ;

pipe_list : cmd
          {
              $$ = $1;
              $$->nextCommand = NULL;
          }
          | cmd PIPE pipe_list
          {
              struct command *pass = $1;
              pass->nextCommand = $3;
              if (pass->nextCommand != NULL) {
                  if (pass->outfile != NULL || pass->nextCommand->infile != NULL) {
                      yyerror("illegal redirection");
                  } else {
                      pass->outfile = pass->nextCommand->infile = "PIPE";
                  }
              }
              $$ = pass;
          }
          ;

cmd	: WORD optargs optredirs
		{
			// make and fill node of type "struct command "
			// grab the linked list for optargs and install it in the structure
			// grab the linked list for optredir and install it in the structure
			if(debugMode)		
				printf("I found a command and it is '%s'\n", $1);

			struct command *pass = (struct command *) MallocZ(sizeof(struct command));
			struct args *args; struct redirs *redirs; 
			pass -> command = pass -> argv[0] = $1[0] == '"' || $1[0] == '\'' ? removeQuots($1) : $1;
			pass -> argc = 1;
 			pass -> line_number = lines + 1;
			for (args = $2; args; args = args->nextCommand) {
				pass -> argv[pass -> argc] = args -> arg;
					pass -> var_exp_flag = args -> var_exp_flag;

				++(pass -> argc);
			}

			for (redirs = $3; redirs; redirs = redirs->nextCommand) {
				switch (redirs -> redir_token)
				{
					case REDIR_INPUT:
						if(pass -> infile != NULL){
							yyerror("illegal redirection");
						}
						else{
							pass -> infile = redirs -> filename;
						}
						break; 
					case REDIR_OUTPUT:
						if(pass -> outfile != NULL){
							yyerror("illegal redirection");
						}
						else{
							pass -> outfile = redirs -> filename;
						}
						break; 
					case REDIR_OUTPUT_APPEND:
						if(pass -> outfile != NULL){
							yyerror("illegal redirection");
						}
						else{
							pass -> outfile = redirs -> filename;
							pass -> output_append = 't';
						}
						break; 
					case REDIR_ERROR:
						if(pass -> errorLogFile != NULL){
							yyerror("illegal redirection");
						}
						else{
							pass -> errorLogFile = redirs -> filename;
						}
						break; 
					case REDIR_ERROR_APPEND:
						if(pass -> errorLogFile != NULL){
							yyerror("illegal redirection");
						}
						else{
							pass -> errorLogFile = redirs -> filename;
							pass -> error_append = 't';
						}
						break; 
					default:
						break;
				}
			}
			if(debugMode)
				printf("infile %s\t outfile %s\t errorLogFile %s\n", pass->infile, pass->outfile, pass->errorLogFile);
		
			pass -> nextCommand = NULL;

			if(debugMode && $2 != NULL)
				printf("I found a optarg and it is '%s'\n", $2 -> arg);
			if(debugMode && $3 != NULL)
				printf("I found a redir and it is '%s'\n", $3 -> filename);
			$$ = pass;
		} 
		| VAR_QUOTE {
			struct command *pass = (struct command *) MallocZ(sizeof(struct command));
					
			pass -> command = removeQuots($1);
			
			pass -> var_flag = 't';
			pass -> line_number = lines + 1;
			$$ = pass;
		}
		| VAR {
			struct command *pass = (struct command *) MallocZ(sizeof(struct command));
					
			pass -> command = $1;
			
			pass -> var_flag = 't';
			pass -> line_number = lines + 1;
			$$ = pass;
		}
	;

// these 2 rules are for "optional arguments".  They should allow one or more "arg"s
// and assemble them into a linked list of type "struct args" and return it upstead
optargs : arg optargs
			{ 
				struct args *pass = $1;//(struct args *) MallocZ(sizeof(struct args));
				pass -> nextCommand = $2;
				if(debugMode)
					printf("arg %s\n", pass->arg);
				
				if(debugMode && pass -> nextCommand == NULL){
					printf("nextCommand is null\n");
				}else if(debugMode){
					printf("nextCommand is %s\n", pass -> nextCommand -> arg);
				}

				$$ = pass;
			}
		|	
			{ $$ = NULL; // no more args 
			}
		;
arg		: WORD
		{
			// make a node for type "struct args" and pass it upsteam
			struct args *pass = (struct args *) MallocZ(sizeof(struct args));
			if($1[0]=='"' || $1[0]=='\''){
				pass -> arg = removeQuots($1);
			}else{
				pass -> arg = $1;
			}

			$$ = pass; 
			if(debugMode)
				printf("WORD'%s'\n", $1);

		} | VAR_WORD {
			struct args *pass = (struct args *) MallocZ(sizeof(struct args));
			pass -> arg = $1;
			pass -> var_exp_flag = 't';

			$$ = pass; 
			if(debugMode)
				printf("VAR_WORD'%s'\n", $1);
		}
		;


// these 2 rules are for "optional redirection".  They should  allow one or more sets of 
// redirection commands from the rule "redir"
// and assemble them into a linked list of type "struct redir" and return it upstead
optredirs : redir optredirs
			{ 
				struct redirs *pass = $1;
				pass -> nextCommand = $2;

				if(debugMode)
					printf("filename upstream %s\n", pass -> filename);

				if(debugMode && pass -> nextCommand == NULL){
					printf("nextCommand is null\n");
				}else if(debugMode){
					printf("nextCommand is %s\n", pass -> nextCommand -> filename);
				}

				$$ = pass;
			}
		|
			{ $$ = NULL; // no more redirection 
			}
		;
// just as a possible example
redir	: INFILE WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_INPUT);
			
			if(debugMode)
				printf("infile %s\n", pass -> filename);
			
			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream
		}
	|	INFILE VAR_WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_INPUT);
			
			if(debugMode)
				printf("infile %s\n", pass -> filename);
			
			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream
		}
	|
		OUTFILE WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_OUTPUT);

			if(debugMode)
				printf("outfile %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	OUTFILE VAR_WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_OUTPUT);

			if(debugMode)
				printf("outfile %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|
		OUTFILE_APPEND WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_OUTPUT_APPEND);
			
			if(debugMode)
				printf("OUTFILE_APPEND %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	
		OUTFILE_APPEND VAR_WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_OUTPUT_APPEND);
			
			if(debugMode)
				printf("OUTFILE_APPEND %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	
		errorLogFile WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_ERROR);
			
			if(debugMode)
				printf("errorLogFile %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	
		errorLogFile VAR_WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_ERROR);
			
			if(debugMode)
				printf("errorLogFile %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	
		errorLogFile_APPEND WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_ERROR_APPEND);

			if(debugMode)
				printf("errorLogFile_APPEND %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
	|	
		errorLogFile_APPEND VAR_WORD
		{
			struct redirs *pass = PrepareRedir($2, REDIR_ERROR_APPEND);

			if(debugMode)
				printf("errorLogFile_APPEND %s\n", pass -> filename);

			$$ = pass;  // build a data structure of type struct redirs for this and pass it upstream

		}
		;

%%

// remove double quotes 
/* char *removeQuots(char* text){
	int len = strlen(text)- 2;
	char *word = (char *) MallocZ(len + 1);
	
	for(int j=0; j < len; j++){
		word[j] = text[j+1];
		}
		word[len] = '\0';

	return word;
} */

void
yyerror(const char *error_string, ...)
{
    va_list ap;
    int line_nmb(void);

    FILE *f = stdout;

    va_start(ap,error_string);

    ++synerrors;
	++synerror;

    fprintf(f,"Error on line %d: ", lines+1);
    vfprintf(f,error_string,ap);

    fprintf(f,"\n");
    va_end(ap);
}


// builds redir model
struct redirs *PrepareRedir(char *filename, int redirToken)
{
	struct redirs *pass = (struct redirs *) MallocZ(sizeof(struct redirs));
	pass -> redir_token = redirToken;
	pass -> filename = strndup(filename, strlen(filename));
	pass -> nextCommand = NULL;
	return (pass);
}


char *removeQuots(char* text){
	int len = strlen(text)- 2;
	char *word = (char *) MallocZ(len + 1);
	int i=0;
	int j=0;

	while(i<len){
		if(j==0 && (text[i] == '\'' || text[i] == '"')){
			j++;
		} else{
			word[i] = text[i+j];
			i++;
		}
	}
	word[len] = '\0';

	return word;
}

void *
MallocZ (int nbytes)
{
    char *ptr = malloc(nbytes);  // use the real routine
    if (ptr == NULL)
	{
	    perror ("MallocZ failed, fatal\n");
	    exit (66);
	}

	// initialize the space to all zeroes
    memset (ptr, '\00', nbytes);

    return (ptr);
}
