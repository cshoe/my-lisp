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

#define LASSERT(args, cond, err) if (!(cond)) {lval_del(args); return lval_err(err);}

/* Possible lval types */
enum { LVAL_NUM, LVAL_ERR , LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR};


/* Declare LISP val struct */
typedef struct lval{
    int type;
    long num;

    char* err;
    char* sym;

    /* Count and pointer to a list of lval* */
    int count;
    struct lval** cell;
} lval;


lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}


lval* lval_err(char* m) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}


lval* lval_sym(char* s) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}


lval* lval_sexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval* lval_qexpr(void) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval* v) {
    switch(v->type) {
        case LVAL_ERR:
            free(v->err);
            break;

        case LVAL_SYM:
            free(v->sym);
            break;

        case LVAL_QEXPR:
        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
        break;
    }
    free(v);
}


lval* lval_add(lval* v, lval* x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    v->cell[v->count-1] = x;
    return v;
}


lval* lval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("Invalid number.");
}


lval* lval_read(mpc_ast_t* t) {
    /* If Symbol or Number, convert and return. */
    if (strstr(t->tag, "number")) { return lval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }

    /* If root (>) or sexpr then create an empty list */
    lval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = lval_sexpr(); }
    if (strstr(t->tag, "qexpr")) { x = lval_qexpr(); }

    /* Fill in this list with any valid expression contained within */
    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
        if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
    putchar(open);
    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count-1)) {
            putchar(' ');
        }
    }
    putchar(close);
}


/* Print an lval */
void lval_print(lval* v) {
    switch(v->type) {
        case LVAL_NUM:
            printf("%li", v->num);
            break;

        case LVAL_ERR:
            printf("Error: %s", v->err);
            break;

        case LVAL_SYM:
            printf("%s", v->sym);
            break;

        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;

        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
    }
}


/* Print an lval followed by a newline */
void lval_println(lval* v) {
    lval_print(v);
    putchar('\n');
}

lval* lval_pop(lval* v, int index) {
    // get the item at index
    lval* to_pop = v->cell[index];

    /* Shift memory to cover up the spot we're taking */
    memmove(&v->cell[index], &v->cell[index+1], sizeof(lval*) * (v->count-index-1));

    v->count--;

    /* Reallocate memory. The order of the array has been reconstructed after
     * the pop the memove call above.
     */
    v->cell = realloc(v->cell, sizeof(lval*) * v->count);
    return to_pop;
}

lval* lval_take(lval* v, int index) {
    lval* x = lval_pop(v, index);
    lval_del(v);
    return x;
}


lval* lval_join(lval* x, lval* y) {
    /* Pop everything from y and add it to x */
    while (y->count) {
        x = lval_add(x, lval_pop(y, 0));
    }

    /* Cleanup the now empty */
    lval_del(y);
    return x;
}

lval* lval_eval(lval* v);
lval* builtin(lval* a, char* op);

lval* lval_eval_sexpr(lval* v) {

    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    /* Error checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Check for empty expression */
    if (v->count == 0) { return v; }

    /* Check for single expression */
    if (v-> count == 1) { return lval_take(v, 0); }

    /* Ensure first element is a symbol */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_SYM) {
        /* We don't have a symbol so we need to cleanup and return an error. */
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression does not start with a symbol!");
    }

    /* Call builtin with operator */
    lval* result = builtin(v, f->sym);
    lval_del(f);
    return result;
}

lval* lval_eval(lval* v) {
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(v);
    }

    /* all other types remain unchanged */
    return v;
}

lval* builtin_tail(lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!"); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed '{}'!");

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lval* a) {
    LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!"); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(x);
}

lval* builtin_join(lval* a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, (a->cell[i]->type == LVAL_QEXPR), "Function 'join' passed incorrect type!");
    }

    /* Remove the first q-exp. All other q-exps will have each of their elements
     * popped and added to it. */
    lval* x = lval_pop(a, 0);
    while(a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval* builtin_op(lval* a, char* op) {

    /* Make sure all arguments are numbers */
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-numbers!");
        }
    }

    /* Pop the first element */
    lval* x = lval_pop(a, 0);

    /* If no arguments and op is substraction, perform negation */
    if (strcmp(op, "-") == 0 && a->count == 0) { x->num = -x->num; }

    while (a->count > 0) {
        /* Pop the next element */
        lval* y = lval_pop(a, 0);

        /* Perform operation */
        if (strcmp(op, "+") == 0) {
            x->num += y->num;
        }

        if (strcmp(op, "-") == 0) {
            x->num -= y->num;
        }

        if (strcmp(op, "*") == 0) {
            x->num *= y->num;
        }

        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division by zero!");
                break;
            }
            x->num /= y->num;
        }
        lval_del(y);
    }
    lval_del(a);
    return x;
}

lval* builtin_head(lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments!"); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed '{}'!");
    lval* v = lval_take(a, 0);
    /* Get rid of everything but the first element */
    while(v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_len(lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'len' passed too many arguments!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'len' passed '{}'!");
    return lval_num(a->cell[0]->count);
}

lval* builtin(lval* a, char* func) {
    if (strcmp("list", func) == 0) { return builtin_list(a); }
    if (strcmp("head", func) == 0) { return builtin_head(a); }
    if (strcmp("tail", func) == 0) { return builtin_tail(a); }
    if (strcmp("join", func) == 0) { return builtin_join(a); }
    if (strcmp("eval", func) == 0) { return builtin_eval(a); }
    if (strcmp("len", func) == 0) { return builtin_len(a); }
    if (strstr("+-/*", func)) { return builtin_op(a, func); }
    lval_del(a);
    return lval_err("Unknown Function!");
}

int main(int argc, char** argv) {

    /* Create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    mpca_lang(MPCA_LANG_DEFAULT,
      "                                                     \
        number   : /-?[0-9]+/ ;                             \
        symbol   : \"len\" | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' | '%' | '^' ;      \
        sexpr    : '(' <expr>* ')' ;                        \
        qexpr    : '{' <expr>* '}' ;                        \
        expr     : <number> | <symbol> | <sexpr> | <qexpr>;\
        lispy    : /^/ <expr>+ /$/ ;                        \
      ",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.4");
    puts("Press Ctrl+c to Exit");
    puts("And as always, have fun!\n");

    while(1) {
        char* input = readline("crispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
        free(input);
    }
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
