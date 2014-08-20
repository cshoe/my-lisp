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

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }


/* Forward type declerations */
struct lval;
struct lenv;

typedef struct lval lval;
typedef struct lenv lenv;

/* Possible lval types */
enum { LVAL_NUM, LVAL_ERR , LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

char* ltype_name(int t) {
    switch (t) {
        case LVAL_FUN:
            return "Function";
            break;
        case LVAL_NUM:
            return "Number";
            break;
        case LVAL_ERR:
            return "Error";
            break;
        case LVAL_SYM:
            return "Symbol";
            break;
        case LVAL_SEXPR:
            return "S-Expression";
            break;
        case LVAL_QEXPR:
            return "Q-Expression";
            break;
        default:
            return "Unknown";
            
    }
}

/* Function pointer for builtins */
typedef lval*(*lbuiltin)(lenv*, lval*);

/* Declare LISP val struct */
struct lval{
    int type;
    long num;

    char* err;
    char* sym;
    lbuiltin fun;

    /* Count and pointer to a list of lval* */
    int count;
    struct lval** cell;
};

lval* lval_num(long x) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_NUM;
    v->num = x;
    return v;
}

lval* lval_err(char* fmt, ...) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_ERR;

    va_list va;
    va_start(va, fmt);

    /* Allocate 512 bytes of space */
    v->err = malloc(512);

    /* printf into the error string with a maximum of 511 characters */
    vsnprintf(v->err, 511, fmt, va);
    v->err = realloc(v->err, strlen(v->err+1));

    va_end(va);
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

lval* lval_fun(lbuiltin func) {
    lval* v = malloc(sizeof(lval));
    v->type = LVAL_FUN;
    v->fun = func;
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
        case LVAL_FUN: break;
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
        case LVAL_FUN:
            printf("<function>");
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

lval* lval_copy(lval* v) {
    lval* x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        /* Copy functions and numbers directly */
        case LVAL_FUN: x->fun = v->fun; break;
        case LVAL_NUM: x->num = v->num; break;

        /* Copy strings using malloc and strcpy */
        case LVAL_ERR:
            /* point x->err to a new piece of memory that is the size of v->err + 1 */
            x->err = malloc(strlen(v->err + 1));
            strcpy(x->err, v->err);
            break;
        case LVAL_SYM:
            /* Same things as ERR but act on the sym attr */
            x->sym = malloc(strlen(v->sym + 1));
            strcpy(x->sym, v->sym);
            break;

        /* Copy lists by copying each sub-expression individually */
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            /* Note that the size of an lval pointer is being allocated. Not the size of an lval */
            x->cell = malloc(sizeof(lval*) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }

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

lval* lval_eval(lenv* e, lval* v);
lval* builtin(lval* a, char* op);

lval* lval_eval_sexpr(lenv* e, lval* v) {
    /* Evaluate Children */
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(e, v->cell[i]);
    }

    /* Error checking */
    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
    }

    /* Check for empty expression */
    if (v->count == 0) { return v; }

    /* Check for single expression */
    if (v-> count == 1) { return lval_take(v, 0); }

    /* Ensure first element is a function */
    lval* f = lval_pop(v, 0);
    if (f->type != LVAL_FUN) {
        /* We don't have a function so we need to cleanup and return an error. */
        lval_del(v);
        lval_del(f);
        return lval_err("S-expression does not start with a function!");
    }

    /* Call builtin with operator */
    lval* result = f->fun(e, v);
    lval_del(f);
    return result;
}

lval* lenv_get(lenv* e, lval* k);

lval* lval_eval(lenv* e, lval* v) {
    if (v->type == LVAL_SYM) {
        lval* x = lenv_get(e, v);
        lval_del(v);
        return x;
    }
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_eval_sexpr(e, v);
    }

    /* all other types remain unchanged */
    return v;
}

lval* builtin_tail(lenv* e, lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'tail' passed too many arguments!"); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'tail' passed incorrect type!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'tail' passed '{}'!");

    lval* v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval* builtin_list(lenv* e, lval* a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval* builtin_eval(lenv* e, lval* a) {
    LASSERT(a, (a->count == 1), "Function 'eval' passed too many arguments!"); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'eval' passed incorrect type!");

    lval* x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(e, x);
}

lval* builtin_join(lenv* e, lval* a) {
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

lval* builtin_op(lenv* e, lval* a, char* op) {

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

lval* builtin_head(lenv* e, lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'head' passed too many arguments! Got %i, expected %i", a->count, 1); 

    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'head' passed incorrect type!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'head' passed '{}'!");
    lval* v = lval_take(a, 0);
    /* Get rid of everything but the first element */
    while(v->count > 1) { lval_del(lval_pop(v, 1)); }
    return v;
}

lval* builtin_len(lenv* e, lval* a) {
    /* sanity checks */
    LASSERT(a, (a->count == 1), "Function 'len' passed too many arguments!");

    LASSERT(a, (a->cell[0]->count != 0), "Function 'len' passed '{}'!");
    return lval_num(a->cell[0]->count);
}

lval* builtin_add(lenv* e, lval* a) { return builtin_op(e, a, "+"); }
lval* builtin_sub(lenv* e, lval* a) { return builtin_op(e, a, "-"); }
lval* builtin_mul(lenv* e, lval* a) { return builtin_op(e, a, "*"); }
lval* builtin_div(lenv* e, lval* a) { return builtin_op(e, a, "/"); }

/* Declare environment struct. */
struct lenv {
    int count;
    char** syms;
    lval** vals;
};

lenv* lenv_new(void) {
    lenv* e = malloc(sizeof(lenv));
    e->count = 0;
    e->syms = NULL;
    e->vals = NULL;
    return e;
}

void lenv_del(lenv* e) {
    /* Loop through each symbol and val and free/delete */
    for (int i = 0; i < e->count; i++) {
       free(e->syms[i]);
       lval_del(e->vals[i]);
    }
    free(e->syms);
    free(e->vals);
    free(e);
}

lval* lenv_get(lenv* e, lval* k) {
    /* Iterate over all items in the environment */
    for (int i = 0; i < e->count; i++) {
        /* check if the stored string matches the symbol string. If it does, return a copy */ 
        if (strcmp(e->syms[i], k->sym) == 0) {
            return lval_copy(e->vals[i]);
        }
    }
    return lval_err("unbound symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval*v) {
    /*Check to see if the variable already exists */
    for (int i = 0; i < e->count; i++) {
        /* If variable is found, delete val at that position and replace with new value */
        if (strcmp(e->syms[i], k->sym) == 0) {
            lval_del(e->vals[i]);
            e->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry is found, allocate space for a new entry */
    e->count++;
    e->vals = realloc(e->vals, sizeof(lval*) * e->count);
    e->syms = realloc(e->syms, sizeof(char*) * e->count);

    /* Copy contents of lval and symbol string into new location */
    e->vals[e->count - 1] = lval_copy(v);
    e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(e->syms[e->count - 1], k->sym);
}

lval* builtin_def(lenv* e, lval* a) {
    LASSERT(a, (a->cell[0]->type == LVAL_QEXPR), "Function 'def' passed incorrect type!");

    /* First arg is a symbol list*/
    lval* syms = a->cell[0];

    /* Make sure all memebers of syms is in fact a symbol */
    for (int i = 0; i < syms->count; i++) {
        LASSERT(a, (syms->cell[i]->type == LVAL_SYM), "Function 'def' cannot define non-symbol!");
    }

    /* Check for correct number of symbols and values */
    LASSERT(a, (syms->count == a->count-1), "Function 'def' cannot define incorrect number of values to symbols!");

    /* Assign copies to symbols */
    for (int i = 0; i < syms->count; i++) {
        lenv_put(e, syms->cell[i], a->cell[i+1]);
    }

    lval_del(a);
    return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
    lval* key = lval_sym(name);
    lval* value = lval_fun(func);
    lenv_put(e, key, value);
    lval_del(key);
    lval_del(value);
}

void lenv_add_builtins(lenv* e) {
    /* List functions */
    lenv_add_builtin(e, "list", builtin_list);
    lenv_add_builtin(e, "head", builtin_head);
    lenv_add_builtin(e, "tail", builtin_tail);
    lenv_add_builtin(e, "eval", builtin_eval);
    lenv_add_builtin(e, "join", builtin_join);
    lenv_add_builtin(e, "len", builtin_len);

    lenv_add_builtin(e, "def", builtin_def);

    /* Math functions */
    lenv_add_builtin(e, "+", builtin_add);
    lenv_add_builtin(e, "-", builtin_sub);
    lenv_add_builtin(e, "*", builtin_mul);
    lenv_add_builtin(e, "/", builtin_div);
}


int main(int argc, char** argv) {

    /* Create some parsers */
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t* Sexpr = mpc_new("sexpr");
    mpc_parser_t* Qexpr = mpc_new("qexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Lispy = mpc_new("lispy");

    // symbol   : \"len\" | \"list\" | \"head\" | \"tail\" | \"join\" | \"eval\" | '+' | '-' | '*' | '/' | '%' | '^' ;
    mpca_lang(MPCA_LANG_DEFAULT,
      "                                                     \
        number   : /-?[0-9]+/ ;                             \
        symbol : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;         \
        sexpr    : '(' <expr>* ')' ;                        \
        qexpr    : '{' <expr>* '}' ;                        \
        expr     : <number> | <symbol> | <sexpr> | <qexpr>; \
        lispy    : /^/ <expr>+ /$/ ;                        \
      ",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    /* Print Version and Exit Information */
    puts("Lispy Version 0.0.4");
    puts("Press Ctrl+c to Exit");
    puts("And as always, have fun!\n");

    lenv* e = lenv_new();
    lenv_add_builtins(e);

    while(1) {
        char* input = readline("crispy> ");
        add_history(input);

        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {

            lval* x = lval_eval(e, lval_read(r.output));
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
    lenv_del(e);
    mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
    return 0;
}
