#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

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

/* Possible lval types */
enum { LVAL_NUM, LVAL_ERR };

/* Possible error types */
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };


/* Declare LISP val struct */
typedef struct {
    int type;
    long num;
    int err;
} lval;


lval lval_num(long x) {
    lval v;
    v.type = LVAL_NUM;
    v.num = x;
    return v;
}


lval lval_err(int x) {
    lval v;
    v.type = LVAL_ERR;
    v.err = x;
    return v;
}


/* Print an lval */
void lval_print(lval v) {
    switch(v.type) {
        case LVAL_NUM:
            printf("%li", v.num);
            break;

        case LVAL_ERR:
            switch(v.err) {

                case LERR_DIV_ZERO:
                    printf("ERROR: Division by zero!");
                    break;

                case LERR_BAD_OP:
                    printf("ERROR: Invalid operator!");
                    break;

                case LERR_BAD_NUM:
                    printf("ERROR: Invalid number!");
                    break;
            }
    }
}


/* Print an lval followed by a newline */
void lval_println(lval v) {
    lval_print(v);
    putchar('\n');
}


lval eval_op(lval x, char* op, lval y) {

    /* If either value is an error, just return it */
    if (x.type == LVAL_ERR) {
        return x;
    }
    if (y.type == LVAL_ERR) {
        return y;
    }

    if (strcmp(op, "+") == 0) {
        return lval_num(x.num + y.num);
    }

    if (strcmp(op, "-") == 0) {
        return lval_num(x.num - y.num);
    }

    if (strcmp(op, "*") == 0) {
        return lval_num(x.num * y.num);
    }

    if (strcmp(op, "/") == 0) {
        /* If second operand is zero, return an error */
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    }

    if (strcmp(op, "%") == 0) {
        return lval_num(x.num % y.num);
    }

    if (strcmp(op, "^") == 0) {
        return lval_num(pow(x.num, y.num));
    }

    return lval_err(LERR_BAD_OP);
}


lval eval(mpc_ast_t* t) {
    /* If tagged as number, return it directly, otherwise, expression. */
    if (strstr(t->tag, "number")) {
        errno = 0;
        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    /* 
     * If we reach here, we know we have an expression and the contents
     * of the second child of the expression is the operator.
     */
    char* op = t->children[1]->contents;

    /* Recuuuuuuuuuuursion */
    lval x = eval(t->children[2]);

    int i = 3;
    while(strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}


int main(int argc, char** argv) {

    /* Create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
      "                                                     \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' | '%' | '^';       \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        lispy    : /^/ <operator> <expr>+ /$/ ;             \
    ",
    Number, Operator, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.1");
    puts("Press Ctrl+c to Exit");
    puts("And as always, have fun!\n");

    while(1) {
        char* input = readline("crispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(4, Number, Operator, Expr, Lispy);
    return 0;
}
