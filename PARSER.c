/* recursive descent integer parser that can include variables and funcion calls
*/
#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define NUM_FUNC 100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS 200
#define ID_LEN 31
#define FUNC_CALLS 31
#define PROG_SIZE 10000
#define FOR_NEST 31

enum tok_types {DELIMITER, IDENTIFIER, NUMBER, KEYWORD, TEMP, STRING, BLOCK};

enum tokens {ARG, CHAR, INT, IF, ELSE, FOR, DO, WHILE, SWITCH, RETURN, EOL, FINISHED, END};

enum double_ops {LT=1, LE, GT, GE, EQ, NE};

/* These are constants used to call syntx_err() when a syntax error occurs.
you can add more, if you with. NOTE: SYNTAX is a generic error message used when no other message
is appropriate. 
*/
enum error_msg {
    SYNTAX, UNBAL_PARENS, NO_EXP, EQUALS_EXPECTED, NOT_VAR, PARAM_ERR, SEMI_EXPECTED, UNBAL_BRACES,
    FUNC_UNDEF, TYPE_EXPECTED, NEST_FUNC, RET_NOCALL, PAREN_EXPECTED, WHILE_EXPECTED, QUOTE_EXPECTED,
    NOT_TEMP, TOO_MANY_LVARS 
};

extern char *prog; /* current position in source code*/
extern char *p_buf; /* points to the start of the program's load area*/
extern jmp_buf e_buf; /* keeps the envoronment for longjump()*/

/* an array of this structure maintains the info related to global variables
*/
extern struct var_type{
    char var_name[32];
    int var_type;
    int value;
}global_vars[NUM_GLOBAL_VARS];

/* this is the function call stack*/
extern struct func_type{
    char func_name[32];
    char *loc; /* function position/location in the source code*/
}func_stack[NUM_FUNC];

/* keyword table*/
extern struct commands{
    char command[20];
    char tok;
}table[];

/* functions of the 'standard' library are declared here to be used in the intern function table
*/
int call_getche(void), call_putch(void), call_puts(void), print(void),getnum(void);

struct intern_func_type{
    char *f_name; /* function name*/
    int (*p)(); /*function pointer*/
}intern_func[] = {
    "getche", call_getche,
    "putch", call_putch,
    "puts", call_puts,
    "print", print,
    "getnum", getnum,
    "",0 /* NULL ends the list*/
};

extern char token[80]; /* string representation of token*/
extern char token_type; /* type of token*/
extern char tok; /* intern representation of token*/

extern int ret_value; /* function return value*/

void eval_exp(int *value), eval_exp1(int *value);
void eval_exp2(int *value);
void eval_exp3(int *value), eval_exp4(int *value);
void eval_exp5(int *value), atom(int *value);
void eval_exp0(int *value);
void syntx_err(int error), putback(void);
void assign_var(char *var_name, int value);
int isdelim(char c), look_up(char *s), iswhite(char c);
int find_var(char *s), get_token(void);
int internal_func(char *s);
int is_var(char *s);
char *find_func(char *name);
void call(void);

/*parser entry point*/
void eval_exp(int *value){
    get_token();
    if(!*token){
        syntx_err(NO_EXP);
        return;
    }
    if(!*token==';'){
        *value = 0; /*empty expression*/
        return;
    }
    eval_exp0(value);
    putback(); /*Returns last token read into input*/
}

/* process and assignment expression*/
void eval_exp0(int *value){
    char temp[ID_LEN]; /* keeps the name of the variable that is receiving the assignment*/
    register int temp_tok;
    if(token_type == IDENTIFIER){
        if(is_var(token)){ /* if is it a variable, check for assignment*/
            strcpy(temp,token);
            temp_tok = token_type;
            get_token();
            if(*token == '='){ /*is an assignment*/
                get_token();
                eval_exp0(value); /*get value to assign*/
                assign_var(temp, *value); /*assigns the value*/
                return;
            }else{ /*not an assignment*/
                putback(); /*restore original token*/
                strcpy(token,temp);
                token_type = temp_tok;
            }
        }
    }
    eval_exp1(value);
}

/* this array is used by eval_exp1(). 
Some compilers don't allow array initialization inside a function, so here it is declared globally
*/
char relops[7] = {
    LT, LE, GT, GE, EQ, NE, 0
};

/*process relational operatos*/
void eval_exp1(int *value){
    int partial_value;
    register char op;

    eval_exp2(value);
    op = *token;
    if(strchr(relops,op)){
        get_token();
        eval_exp2(&partial_value);
        switch(op){ /*performs relational operation*/
            case LT:
                *value = *value < partial_value;
                break;
            case LE:
                *value = *value <= partial_value;
                break;
            case GT:
                *value = *value > partial_value;
                break;
            case GE:
                *value = *value >= partial_value;
                break;
            case EQ:
                *value = *value == partial_value;
                break;
            case NE:
                *value = *value != partial_value;
                break;
        }
    }
}

/* adds or subtracts two terms*/
void eval_exp2(int *value){
    register char op;
    int partial_value;

    eval_exp3(value);
    while((op = *token) == '+' || op == '-'){
        get_token();
        eval_exp3(&partial_value);
        switch(op){ /*adds or subtracts*/
            case '-':
                *value = *value - partial_value;
                break;
            case '+':
                *value = *value + partial_value;
                break;
        }
    }
}

/* multiply or divide two factors*/
void eval_exp3(int *value){
    register char op;
    int partial_value, t;

    eval_exp4(value);
    while((op = *token) == '*' || op == '/' || op == '%'){
        get_token();
        eval_exp4(&partial_value);
        switch(op){ /*multiply, divide or mod*/
            case '*':
                *value = *value * partial_value;
                break;
            case '/':
                *value = *value / partial_value;
                break;
            case '%':
                t = (*value) / partial_value;
                *value = *value - (t * partial_value);
                break;
        }
    }
}

/* is a unary '+' or '-'*/
void eval_exp4(int *value){
    register char op;
    op = '\0';
    if(*token == '+' || *token == '-'){
        op = *token;
        get_token();
    }
    eval_exp5(value);
    if(op)
        if(op == '-') *value = -(*value);
}

/* process parentheses expressions*/
void eval_exp5(int *value){
    if((*token == '(')){
        get_token();
        eval_exp0(value); /*get subexpression*/
        if(*token != ')') syntx_err(PAREN_EXPECTED);
        get_token();
    }else{
        atom(value);
    }
}

/* Find number, variable or function value*/
void atom(int *value){
    int i;
    switch(token_type){
        case IDENTIFIER:
            i = internal_func(token);
            if(i != -1){ /*calls the function from the 'standard' library*/
                *value = (*intern_func[i].p)();
            }else if(find_func(token)){ /*calls a user defined function*/
                call();
                *value = ret_value;
            }else *value = find_var(token); /*get variable value*/
            get_token();
            return;
        case NUMBER: /*its a number constant*/
            *value = atoi(token);
            get_token();
            return;
        case DELIMITER: /*check if it is a character constant*/
            if(*token =='\''){
                *value = *prog;
                prog++;
                if(*prog!='\'') syntx_err(QUOTE_EXPECTED);
                prog++;
                get_token();
            }
            return;
        default:
            if(*token ==')') return; /*process empty expression*/
            else syntx_err(SYNTAX); /*syntax error*/
    }
}

/*shows an error message*/
void syntx_err(int error){
    char *p, *temp;
    int linecount = 0;
    register int i;
    static char *e[] = {
        "Syntax Errpr",
        "Unbalanced Parentheses",
        "Missing Expression",
        "Equal sign expected",
        "Not a variable",
        "Parameter Error",
        "Expected point-and-comma",
        "Unbalanced brackets",
        "Function not defined",
        "Expected type identifier",
        "Excessive nested function calls",
        "No return call",
        "Expected parenthesis",
        "Expected while",
        "Expected Quote",
        "Not a string",
        "Excessive local variables"
    };
    printf("%s",e[error]);
    p = p_buf;
    while(p != prog){ /*finds the error line*/
        p++;
        if(*p == '\r'){
            linecount++;
        }
    }
    printf(" on line %d\n",linecount);

    temp = p;
    for(i=0; i<20 && p>p_buf && *p != '\n';i++, p--);
    for(i=0; i<30 && p<=temp; i++, p++) printf("%c",*p);

    longjmp(e_buf, 0); /*returns to a safe spot*/
}

/*gets a token*/
int get_token(void){
    register char *temp;
    token_type = 0;
    tok = 0;

    temp = token;
    *temp = '\0';

    /*ignores empty spaces*/
    while(iswhite(*prog) && *prog) ++prog;

    if(*prog=='\r'){
        ++prog;
        ++prog;
        /*ignores empty spaces*/
        while(iswhite(*prog) && *prog) ++prog;
    }
    if(*prog == '\0'){ /*end of file*/
        *token = '\0';
        tok = FINISHED;
        return (token_type = DELIMITER);
    }

    if(strchr("{}", *prog)){ /*block delimiter*/
        *temp = *prog;
        temp++;
        *temp='\0';
        prog++;
        return (token_type = BLOCK);
    }

    /*look for comments*/
    if(*prog == '/')
        if(*(prog+1) == '*'){ /*is a comment*/
            prog += 2;
            do{ /*finds the end of comment*/
                while(*prog != '*') prog++;
                prog++;
            }while (*prog != '/');
            prog++;
        }
    if(strchr("!<>=",*prog)){ /*Is or can be a relational operator*/
        switch(*prog){
            case '=':
                if(*(prog+1)=='='){
                    prog++; prog++;
                    *temp = EQ; temp++;
                    *temp = EQ; temp++;
                    *temp = '\0';
                }
                break;
            case '!':
                if(*(prog+1)=='='){
                    prog++; prog++;
                    *temp = NE; temp++;
                    *temp = NE; temp++;
                    *temp = '\0';
                }
                break;
            case '<':
                if(*(prog+1)=='='){
                    prog++; prog++;
                    *temp = LE; temp++;
                    *temp = LE;
                }else{
                    prog++;
                    *temp = LT;
                }
                temp++;
                *temp = '\0';
                break;
            case '>':
                if(*(prog+1)=='='){
                    prog++; prog++;
                    *temp = GE; temp++;
                    *temp = GE;
                }else{
                    prog++;
                    *temp = GT;
                }
                temp++;
                *temp = '\0';
                break;
        }
        if(*token) return (token_type = DELIMITER);
    }
    if(strchr("+-*^/%=;(),'", *prog)){ /*delimiter*/
        *temp = *prog;
        prog++; /*moves into next position*/
        temp++;
        *temp = '\0';
        return (token_type = DELIMITER);
    }

    if(*prog == '"'){ /*String in quotation marks*/
        prog++;
        while(*prog != '"' && *prog != '\r') *temp++ = *prog++;
        if(*prog == '\r') syntx_err(SYNTAX);
        prog++; *temp = '\0';
        return (token_type = STRING);
    }

    if(isdigit(*prog)){ /*number*/
        while(!isdelim(*prog)) *temp++ = *prog++;
        *temp = '\0';
        return (token_type = NUMBER);
    }

    if(isalpha(*prog)){ /*variable or command*/
        while(!isdelim(*prog)) *temp++ = *prog++;
        token_type = TEMP;
    }
    *temp = '\0';

    /*check if a string is a command or a variable*/
    if(token_type == TEMP){
        tok = look_up(token); /*Converts to internal representation*/
        if(tok) token_type = KEYWORD; /*is a keyword*/
        else token_type = IDENTIFIER;
    }
    return token_type;
}

/*returns a token to input*/
void putback(void){
    char *t;

    t = token;
    for(; *t; t++) prog--;
}

/*Search for the internal representation of a token in the token table*/
int look_up(char *s){
    register int i;
    char *p;

    /*converts to lowecase*/
    p = s;
    while(*p){ *p = tolower(*p); p++; }

    /*verifies if the token is in the table*/
    for(i = 0; *table[i].command; i++)
        if(!strcmp(table[i].command,s)) return table[i].tok;
    return 0; /*unkown command*/
}

/*Returns standard funcion index or -1 if it can't be found*/
int internal_func(char *s){
    int i;
    for(i=0; intern_func[i].f_name[0]; i++){
        if(!strcmp(intern_func[i].f_name,s)) return i;
    }
    return -1;
}

/*returns true if c is a delimiter*/
int isdelim(char c){
    if(strchr(" !;,+-<>'/*%^=(){}",c) || c==9 || c=='\r' || c==0) return 1;
    return 0;
}

/*returns 1 if it is a space or tab character*/
int iswhite(char c){
    if(c==' ' || c=='\t') return 1;
    return 0;
}