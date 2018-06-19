/*INTERN LIBRARY FUNCTIONS*/
/*ADD MORE BY OWN ACCOUNT*/

#include <conio.h> /*Delete this if your compiler does not support this header*/
#include <stdio.h>
#include <stdlib.h>

extern char *prog; /*current position in source code*/
extern char token[80]; /*String representation of Token*/
extern char token_type; /*type of token*/
extern char tok; /*Intern representation of token*/

enum tok_types {DELMITER, IDENTIFIER, NUMBER, KEYWORD, TEMP, STRING, BLOCK};

/* These are constants used to call syntx_err() when a syntax error occurs.
you can add more, if you with. NOTE: SYNTAX is a generic error message used when no other message
is appropriate.*/
enum error_msg {
    SYNTAX, UNBAL_PARENS, NO_EXP, EQUALS_EXPECTED, NOT_VAR, PARAM_ERR, SEMI_EXPECTED, UNBAL_BRACES,
    FUNC_UNDEF, TYPE_EXPECTED, NEST_FUNC, RET_NOCALL, PAREN_EXPECTED, WHILE_EXPECTED, QUOTE_EXPECTED,
    NOT_TEMP, TOO_MANY_LVARS 
};

int get_token(void);
void syntx_err(int error), eval_exp(int *result);
void putback(void);

/*Gets a character from the console, (use getchar() if your compiler doesn`t support getche()*/
int call_getche(void){
    char ch;
    ch = getche();
    while(*prog!=')') prog++;
    prog++; /*moves to end of line*/
    return ch;
}

/*shows a character on console*/
int call_putch(void){
    int value;
    eval_exp(&value);
    printf("%c",value);
    return value;
}

/*calls puts()*/
int call_puts(void){
    get_token();
    if(*token!='(') syntx_err(PAREN_EXPECTED);
    get_token();
    if(token_type!=STRING) syntx_err(QUOTE_EXPECTED);
    puts(token);
    get_token();
    if(*token!=')') syntx_err(PAREN_EXPECTED);

    get_token();
    if(*token !=';') syntx_err(SEMI_EXPECTED);
    putback();
    return 0;
}

/*A built-in output function for the console*/
int print(void){
    int i;

    get_token();
    if(*token!='(') syntx_err(PAREN_EXPECTED);
    get_token();
    if(token_type==STRING){ /*Showing a string*/
        printf("%s ",token);
    }else{ /*showing a number*/
        putback();
        eval_exp(&i);
        printf("%d ",i);
    }

    get_token();

    if(*token!=')') syntx_err(PAREN_EXPECTED);

    get_token();

    if(*token!=';') syntx_err(SEMI_EXPECTED);
    putback();
    return 0;
}

/*reads an integer value from the console*/
int getnum(void){
    char s[80];
    gets(s);
    while(*prog!=')') prog++;
    prog++; /*moves to end of line*/
    return atoi(s);
}