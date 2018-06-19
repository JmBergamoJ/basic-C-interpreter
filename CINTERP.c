/*A basic C interpreter*/

#include <stdio.h>
#include <setjmp.h>
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define NUM_FUNC 100
#define NUM_GLOBAL_VARS 100
#define NUM_LOCAL_VARS 200
#define NUM_BLOCK 100
#define ID_LEN 31
#define FUNC_CALLS 31
#define NUM_PARAMS 31
#define PROG_SIZE 10000
#define LOOP_NEST 31

enum tok_types {DELMITER, IDENTIFIER, NUMBER, KEYWORD, TEMP, STRING, BLOCK};

/*you can add more C tokens here*/
enum tokens {ARG, CHAR, INT, IF, ELSE, FOR, DO, WHILE, SWITCH, RETURN, EOL, FINISHED, END};

/*you can add more operators here*/
enum double_ops {LT=1, LE, GT, GE, EQ, NE};

/* These are constants used to call syntx_err() when a syntax error occurs.
you can add more, if you with. NOTE: SYNTAX is a generic error message used when no other message
is appropriate. */
enum error_msg {
    SYNTAX, UNBAL_PARENS, NO_EXP, EQUALS_EXPECTED, NOT_VAR, PARAM_ERR, SEMI_EXPECTED, UNBAL_BRACES,
    FUNC_UNDEF, TYPE_EXPECTED, NEST_FUNC, RET_NOCALL, PAREN_EXPECTED, WHILE_EXPECTED, QUOTE_EXPECTED,
    NOT_TEMP, TOO_MANY_LVARS 
};


char *prog; /*current position in source code*/
char *p_buf; /*points to the start of the program's load area*/

jmp_buf e_buf; /*keeps the envoronment for longjump()*/

/*an array of this structure maintains the info related to global variables*/
struct var_type{
    char var_name[ID_LEN];
    int var_type;
    int value;
}global_vars[NUM_GLOBAL_VARS];

struct var_type local_var_stack[NUM_LOCAL_VARS];

struct func_type{
    char func_name[ID_LEN];
    char *loc; /* function position/location in the source code*/
}func_table[NUM_FUNC];

int call_stack[NUM_FUNC];

struct commands{ /*keyword search table*/
    char command[20];
    char tok;
}table[] = {
    "if", IF,
    "else", ELSE,
    "for", FOR,
    "do", DO,
    "while", WHILE,
    "char", CHAR,
    "int", INT,
    "return", RETURN,
    "end", END,
    "", END /*NULL ends the list*/
};

char token[80];
char token_type, tok;

int functos; /*function call top of stack*/

int func_index; /*function table index*/
int gvar_index; /*global variable table index*/
int lvartos; /*local variable table index*/

int ret_value; /*function return value*/

void print(void), prescan(void);
void decl_global(void), call(void), putback(void);
void decl_local(void), local_push(struct var_type i);
void eval_exp(int *value), syntx_err(int error);
void exec_if(void), find_eob(void), exec_for(void);
void get_params(void), get_args(void);
void exec_while(void), func_push(int i), exec_do(void);
void assign_var(char *var_name, int value);
int load_program(char *p, char *fname), find_var(char *s);
void interp_block(void), func_ret(void);
int func_pop(void), is_var(char *s), get_token();

char *find_func(char *name);

int main(int argc, char*argv[]){
    if(argc != 2){
        printf("Use: interp <filename>\n");
        exit(1);
    }

    /*Allocs memory to load the program*/
    if((p_buf = (char *)malloc(PROG_SIZE))== NULL){
        printf("Allocation failed\n");
        exit(1);
    }

    /*loads the source code*/
    if(!load_program(p_buf, argv[1])) exit(1);
    if(setjmp(e_buf)) exit(1); /*initialize buffer to longjump()*/
    /*sets program pointer to start of load program*/
    prog = p_buf;
    prescan(); /*finds the position of all the functions in the source code, as well as declaring any global variables*/
    gvar_index = 0; /*initialize global variable index*/
    lvartos = 0; /*initialize local variable index*/
    functos = 0; /*initialize function call top of stack*/

    /*prepares 'main' function call*/
    prog = find_func("main"); /*finds the start of funcion*/

    prog--; /*moves back into (*/
    strcpy(token, "main");
    call(); /*initiates interpretation of 'main()'*/
    return 0;
}

/*It interprets a single command or block of code. 
When interp_block () returns from its initial call, 
the final key (or return) was found in main ().*/
void interp_block(void){
    int value;
    char block = 0;

    do{
        token_type = get_token();

        /*If interpreting a single command, return to find the first semicolon*/
        /*See what type of token is ready*/
        if(token_type == IDENTIFIER){
            /*Not a keyword, processes expression*/
            putback();/*returns token into input for further processing by eval_exp ()*/
            eval_exp(&value); /*Processes expression*/
            if(*token != ';') {
                syntx_err(SEMI_EXPECTED);
            }
        }else if(token_type == BLOCK){ /*If block delimiter*/
            if(*token == '{') /*is a block*/
                block = 1; /*interpreting a block, not a command*/
            else return; /*is a closing bracket, if so, returns*/
        }else{ /*is a keyword*/
            switch(tok){
                case CHAR:
                case INT: /*declares local variables*/
                    putback();
                    decl_local();
                    break;
                case RETURN: /*returns function call*/
                    func_ret();
                    return;
                case IF: /*processes an IF statement*/
                    exec_if();
                    break;
                case ELSE:
                    find_eob(); /*finds the end of an ELSE block and continues*/
                    break;
                case WHILE: /*processes a WHILE loop*/
                    exec_while();
                    break;
                case DO: /*processes a DO-WHILE loop*/
                    exec_do();
                    break;
                case FOR: /*processes a FOR loop*/
                    exec_for();
                    break;
                case END:
                    exit(0);
            }
        }
    }while(tok != FINISHED && block);
}

/*load the program*/
int load_program(char *p, char *fname){
    FILE *fp;
    int i=0;

    if((fp = fopen(fname, "rb")) == NULL) return 0;

    i = 0;
    do{
        *p = getc(fp);
        p++; i++;
    }while(!feof(fp) && i < PROG_SIZE);
    if(*(p-2) == 0x1a) *(p-2) = '\0'; /*ends the source code with a NULL character*/
    else *(p-1) = '\0';
    fclose(fp);
    return 1;
}

/*Finds the position of all functions in the program and stores all global variables*/
void prescan(void){
    char *p;
    char temp[32];
    int brace = 0; /*When 0, this variable indicates that the current position in the source code is
                     outside of any function*/
    
    p = prog;
    func_index = 0;
    do{
        while(brace){ /*ignores code inside functions*/
            get_token();
            if(*token == '{') brace++;
            if(*token == '}') brace--;
        }
        get_token();

        if(tok == CHAR || tok == INT){ /*is a global variable*/
            putback();
            decl_global();
        }else if(token_type == IDENTIFIER){
            strcpy(temp,token);
            get_token();
            if(*token=='('){ /*have to be a function*/
                func_table[func_index].loc = prog;
                strcpy(func_table[func_index].func_name, temp);
                func_index++;
                while(*prog!=')') prog++;
                prog++;
                /*now prog points to the start of the function*/
            }else putback();
        }else if(*token=='{') brace++;
    }while(tok != FINISHED);
    prog = p;
}


/*Returns the entry point of the specified function. Returns NULL if not found*/
char *find_func(char *name){
    register int i;

    for(i = 0; i<func_index; i++)
        if(!strcmp(name, func_table[i].func_name))
            return func_table[i].loc;
    return NULL;
}

/*declares a global variable*/
void decl_global(void){
    
    get_token(); /*gets type*/

    global_vars[gvar_index].var_type = tok;
    global_vars[gvar_index].value = 0; /*initialize value as 0*/

    do{ /*Process comma-separated list*/
        get_token();
        strcpy(global_vars[gvar_index].var_name,token);
        get_token();
        gvar_index++;
    }while(*token==',');
    if(*token!=';') {
        syntx_err(SEMI_EXPECTED);
    }
}

/*declares a local variable*/
void decl_local(void){
    struct var_type i;
    get_token(); /*gets type*/
    i.var_type = tok;
    i.value = 0; /*initialize value as 0*/
    do{ /*Process comma-separated list*/
        get_token();/*get variable name*/
        strcpy(i.var_name,token);
        local_push(i);
        get_token();
    }while(*token ==',');
    if(*token!=';') 
    {
        syntx_err(SEMI_EXPECTED);
    }
}

/*calls a function*/
void call(void){
    char *loc, *temp;
    int lvartemp;

    loc = find_func(token); /*find the entry point of the function*/

    if(loc == NULL)
        syntx_err(FUNC_UNDEF); /*undefined function*/
    else{
        lvartemp = lvartos; /*save the current local variable top of stack index*/
        get_args(); /*get function arguments*/
        temp = prog; /*stores return position*/
        func_push(lvartemp); /*stores the current local variable top of stack*/
        prog = loc; /*redefines prog to start of function*/
        get_params(); /*loads the parameters of the function with the values of the arguments*/
        interp_block(); /*interprets the function*/
        prog = temp; /*resets the program pointer to the return address*/
        lvartos = func_pop(); /*resets the local variable stack*/
    }
}

/*Stacks the arguments of a function in the stack of local variables*/
void get_args(void){
    int value, count, temp[NUM_PARAMS];
    struct var_type i;

    count = 0;
    get_token();
    if(*token!='(') syntx_err(PAREN_EXPECTED);

    /*Process a comma-separated list of values*/
    
    do{
        eval_exp(&value);
        temp[count] = value; /*saves temporarily*/
        get_token();
        count++;
    }while(*token==',');
    count--;
    /*now, stacks in local_var_stack in reverse order*/
    for(;count >= 0; count--){
        i.value = temp[count];
        i.var_type = ARG;
        local_push(i);
    }
}

/*Get function parameters*/
void get_params(void){
    struct var_type *p;
    int i;

    i=lvartos=1;
    do{ /*Process comma-separated list of parameters*/
        get_token();
        p = &local_var_stack[i];
        if(*token!=')'){
            if(tok!=INT && tok!=CHAR) syntx_err(TYPE_EXPECTED);
            p->var_type = token_type;
            get_token();

            /*Links parameter name with argument that is already in the stack of local variables*/
            strcpy(p->var_name,token);
            get_token();
            i--;
        }else break;
    }while(*token==',');
    if(*token!=')') syntx_err(PAREN_EXPECTED);
}

/*returns a function*/
void func_ret(void){
    int value;
    value = 0;
    /*finds return value, if any*/
    eval_exp(&value);

    ret_value = value;
}

/*stacks a local variable*/
void local_push(struct var_type i){
    if(lvartos > NUM_LOCAL_VARS)
        syntx_err(TOO_MANY_LVARS);
    local_var_stack[lvartos] = i;
    lvartos++;
}

/*'unstacks' index of local variables*/
int func_pop(void){
    functos--;
    if(functos < 0) syntx_err(RET_NOCALL);
    return (call_stack[functos]);
}

/*Stacks the index of local variable top of stack*/
void func_push(int i){
    if(functos > NUM_FUNC)
        syntx_err(NEST_FUNC);
    call_stack[functos] = i;
    functos++;
}

/*Assigns a value to a variable*/
void assign_var(char *var_name, int value){
    register int i;

    /*First, see if it's a local variable*/
    for(i=lvartos-1; i>=call_stack[functos-1];i--){
        if(!strcmp(local_var_stack[i].var_name,var_name)){
            local_var_stack[i].value = value;
            return;
        }
    }
    if(i < call_stack[functos-1])
    /*If it's not local, try the global variable table*/
        for(i=0;i<NUM_GLOBAL_VARS;i++){
            if(!strcmp(global_vars[i].var_name,var_name)){
                global_vars[i].value = value;
                return;
            }
        }
    syntx_err(NOT_VAR); /*variable not found*/
}

/*finds the value of a variable*/
int find_var(char *s){
    register int i;
    /*first, see if it's a local variable*/
    for(i = lvartos-1;i>=call_stack[lvartos-1];i--)
        if(!strcmp(local_var_stack[i].var_name,s))
            return local_var_stack[i].value;

    /*if it's not loca, try the global variable table*/
    for(i=0; i<NUM_GLOBAL_VARS;i++)
        if(!strcmp(global_vars[i].var_name,s))
            return global_vars[i].value;
    syntx_err(NOT_VAR);/*variable not found*/
}

/*Determines whether an identifier is a variable
Returns 1 if the variable is found, 0 otherwise*/
int is_var(char *s){
    register int i;

    /*first, see if it's a local variable*/
    for(i = lvartos-1;i>=call_stack[lvartos=1];i--)
        if(!strcmp(local_var_stack[i].var_name,token))
            return 1;
        
    /*otherwise, try global variables*/
    for(i=0;i<NUM_GLOBAL_VARS;i++)
        if(!strcmp(global_vars[i].var_name,s))
            return 1;

    return 0;
}

/*executes an IF statement*/
void exec_if(void){
    int cond;
    eval_exp(&cond); /*get leftmost expression*/
    if(cond) { /*is true, then process the IF block*/
        interp_block();
    }else{ /*otherwise, ignores IF block, and then executes ELSE block, if any*/
        find_eob(); /*finds the end of the block*/
        get_token();
        if(tok!=ELSE){
            putback(); /*returns the current token if it's not an ELSE*/
            return;
        }
        interp_block();
    }
}

/*executes a WHILE loop*/
void exec_while(void){
    int cond;
    char *temp;
    putback();
    temp = prog; /*saves the start position of while loop*/
    get_token();
    eval_exp(&cond); /*check conditional expression*/
    if(cond) interp_block(); /*if true, interpret*/
    else{ /*otherwise, ignore loop*/
        find_eob();
        return;
    }
    prog = temp; /*returns to start of loop*/
}

/*executes a DO-WHILE loop*/
void exec_do(void){
    int cond;
    char *temp;

    putback();
    temp = prog; /*saves the start position of do-while loop*/

    get_token(); /*finds the start of block*/
    interp_block(); /*interprets the loo*/
    get_token();
    if(tok!=WHILE) syntx_err(WHILE_EXPECTED);
    eval_exp(&cond); /*check loop condition*/
    if(cond) prog = temp; /*if true, repeat, otherwise, continue*/
}

/*find the end of a block*/
void find_eob(void){
    int brace;
    get_token();
    brace = 1;
    do{
        get_token();
        if(*token=='{') brace++;
        else if(*token=='}') brace--;
    }while(brace);
}

/*executes a FOR loop*/
void exec_for(void){
    int cond;
    char *temp, *temp2;
    int brace;

    get_token();
    eval_exp(&cond); /*initialize expression*/
    if(*token!=';') {
        syntx_err(SEMI_EXPECTED);
    }
    prog++; /*moves after ';'*/
    temp = prog;
    for(;;){
        eval_exp(&cond); /*check condition*/
        if(*token!=';') {
            syntx_err(SEMI_EXPECTED);
        }
        prog++; /*moves after ';'*/
        temp2 = prog;

        /*finds the start of FOR loop*/
        brace = 1;
        while(brace){
            get_token();
            if(*token == '(')brace++;
            if(*token == ')')brace--;
        }
        if(cond) interp_block(); /*if true, interpret*/
        else{ /*otherwise, ignores loop*/
            find_eob();
            return;
        }
        prog = temp2;
        eval_exp(&cond); /*Do the increment*/
        prog = temp; /*back to the start*/
    }
}