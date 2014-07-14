#include <stdio.h>
#include <stdlib.h>

/* If compiling on windows, use these functions */
#ifdef _WIN32

/* Declare a static buffer for user input with max size of 2048 */
static char input[2048];

/* Fake readline function */
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer+1));
    strcpy(cpy, buffer);
    cpy[strlen(cpy)-1] = '\0';
    return cpy;
}

/* Fake add_history function */
void add_history(char* unused) {}

/* If not Windows, include editline headers */
#else

#include <editline/readline.h>

#endif


int main(int argc, char** argv) {

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit");
    puts("And as always, have fun!\n");

    while(1) {
        char* input = readline("crispy> ");
        add_history(input);

        printf("Echo: %s\n", input);
        free(input);
    }
    return 0;

}
