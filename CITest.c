/* Interpreter test program #1.
This program demonstrates all the features of the interpreter 
*/
int i, j; /* global Variables */
char ch;

main(){
    int i, j; /* local variables */;

    puts("Demo #1");
    print_alpha();
    do{
        puts("Input a number (0 - exit): ");
        i = getnum();
        if (i < 0) {
            puts("number needs to be positive, try again");
        }else{
            for (j = 0; j < i; j + 1) {
                print(j);
                puts(" Added is ");
                print(soma(j));
                puts("");
            }
        }
    } while (i != 0);
}

/* sums the values between 0 and num. */
soma(int num){
    int soma_corr;
    soma_corr = 0;
    while (num) {
        soma_corr = soma_corr + num;
        num = num - 1;
    }
    return soma_corr;
}

/* prints the alphabet. */
print_alpha(){
    for (ch = 'A'; ch<='Z'; ch = ch + 1) {
        putch(ch);
    }
    puts("");
}