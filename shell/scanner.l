
%{
/*
 * Simple example lex input file
 *
 * Shawn Ostermann -- Sept 18,2024
 *
 * Daniel Safavi -- Nov 1, 2024
 *
 * Headstart version
 */

#include <string.h>
#include <ctype.h>
#include "parser.tab.h"

void yyerror(const char *s,...);
void yywarn(const char *s,...);
int yywrap(void) {return(1);}

extern int debugMode;
%}

%%

[ \t]+	        { }  /* Just ignore white space */

"\n"            { return(EOLN); } 

"&&"            { return(AND); }
"||"            { return(OR); }
"|"             { return(PIPE); }
"<"             { return(INFILE); }
">>"            { return(OUTFILE_APPEND); }
">"             { return(OUTFILE); }
"2>>"           { return(errorLogFile_APPEND); }
"2>"            { return(errorLogFile); }

[a-zA-Z][a-zA-Z0-9\_]*\=((\')[^'^\n]*(\')|(\")[^"^\n]*(\")) {
    yylval.string = strdup(yytext);
    return(VAR_QUOTE);
}


($\{?[a-zA-Z][a-zA-Z0-9\_]*\}?)|(\")[^"^\n]*(\'?$\{?[a-zA-Z][a-zA-Z0-9\_]*\}?\'?[^"^\n]*)+(\") {
    yylval.string = strdup(yytext);
    return(VAR_WORD);
}

[a-zA-Z][a-zA-Z0-9\_]*\=[a-zA-Z0-9~!@#$%\^&*_\+=/,-\.\(\)\{\}\[\]?:;']+ {
    yylval.string = strdup(yytext);
    return(VAR);
}

(\')[^'^\n]*(\')|(\")[^"^\n]*(\")|[a-zA-Z0-9~!@#$%\^&*_\+=/,-\.\(\)\{\}\[\]?:;']+ {
    yylval.string = strdup(yytext);
    return(WORD);
}

["] {
    yyerror("unterminated string");
}

[<\|>2]+ {
    yyerror("syntax error");
}

.		{
    fprintf(stdout, "scanner: Bad character '%c'\n", *yytext);
}


%%