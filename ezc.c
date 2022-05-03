/******************************************************************************/
/**                                  TODOS                                   **/
/******************************************************************************/

/*
 * [ ] Semantic Analysis
 * [ ] AST to IR-C
 */

/******************************************************************************/
/**                                  UTILS                                   **/
/******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MALLOC_TYPE(_Type_) ((_Type_ *)malloc(sizeof(_Type_)))
#define DUP_OBJ(_Type_, dest, src)\
    ((_Type_*)\
        (dest=MALLOC_TYPE(_Type_),\
        memcpy((void*)(dest),(const void *)(src),sizeof(_Type_))))

#include <stdarg.h>

void
error(char *fmt, ...)
{
    va_list ap;

    printf("[!] ERROR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void
fatal(char *fmt, ...)
{
    va_list ap;

    printf("[!] ERROR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    exit(1);
}

/******************************************************************************/
/**                            STRING INTERNING                              **/
/******************************************************************************/

#include <stddef.h> /* offsetof macro */

typedef struct
StrInterned
{
    struct StrInterned *next;
    int len;
    char str[1];
} StrInterned;

StrInterned *str_intern_table;

StrInterned *
str_intern_create(char *s)
{
    StrInterned *res;
    int s_len = strlen(s);
    res = (StrInterned *)malloc(sizeof(StrInterned) + s_len);
    res->next = 0;
    res->len = s_len;
    strcpy(res->str, s);
    res->str[s_len] = 0;
    return(res);
}

void
str_intern_add(StrInterned *si)
{
    StrInterned *s;

    if(str_intern_table)
    {
        s = str_intern_table;
        while(s->next)
        {
            s = s->next;
        }
        s->next = si;
    }
    else
    {
        str_intern_table = si;
    }
}

int
str_intern_len(char *s)
{
    int res = 0;
    if(s)
    {
        StrInterned *tmp = (StrInterned *)(s - offsetof(StrInterned, str));
        res = tmp->len;
    }
    else
    {
        res = 0;
    }

    return(res);
}

char *
str_intern(char *s)
{
    char *res = 0;
    int found;
    StrInterned *ptr;
    int s_len;

    if(s && *s)
    {
        found = 0;
        ptr = str_intern_table;
        s_len = strlen(s);
        while(!found && ptr)
        {
            found = (ptr->len == s_len && strcmp(ptr->str, s) == 0);
            if(!found)
            {
                ptr = ptr->next;
            }
        }

        if(!found)
        {
            ptr = str_intern_create(s);
            str_intern_add(ptr);
        }

        res = (char *)ptr->str;
    }
    else
    {
        res = 0;
    }

    return(res);
}

/******************************************************************************/
/**                                 TYPES                                    **/
/******************************************************************************/

/*
 * Types:
 *
 * We have 4 built-in types (for now): void, char, int, pointer.
 * Upon these 4 types other composed types can be built (structs, unions).
 *
 * We use type uniquing (you can't have 2 different (int*), they must be
 * the same. In this way we can compare two types by doing a simple ==.
 * 
 */

typedef struct Type Type;

typedef struct
FuncParam
{
    char *id;
    Type *type;
    struct FuncParam *next;
} FuncParam;

FuncParam *
make_func_param(char *id, Type *type)
{
    FuncParam *res;

    res = MALLOC_TYPE(FuncParam);
    if(res)
    {
        res->id = id;
        res->type = type;
        res->next = 0;
    }

    return(res);
}

enum
{
    TYPE_VOID,
    TYPE_CHAR,
    TYPE_INT,
    TYPE_PTR,
    TYPE_FUNC,

    TYPE_COUNT
};

struct
Type
{
    int kind;
    int size;
    struct Type *base_type;
    FuncParam *params;
};

Type _type_void = { TYPE_VOID, 0, 0 };
Type _type_char = { TYPE_CHAR, 1, 0 };
Type _type_int  = { TYPE_INT,  4, 0 };

Type *
type_void()
{
    return(&_type_void);
}

Type *
type_char()
{
    return(&_type_char);
}

Type *
type_int()
{
    return(&_type_int);
}

#define TYPE_PTR_CACHE_SIZE 1000
Type type_ptr_cache[TYPE_PTR_CACHE_SIZE];
int type_ptr_cache_count = 0;

Type *
type_ptr(Type *base_type)
{
    Type *res = 0;
    int i;

    for(i = 0;
        i < type_ptr_cache_count;
        ++i)
    {
        if(type_ptr_cache[i].base_type == base_type)
        {
            res = &(type_ptr_cache[i]);
            break;
        }
    }

    if (!res)
    {
        res = &(type_ptr_cache[type_ptr_cache_count]);
        ++type_ptr_cache_count;
        res->kind = TYPE_PTR;
        res->size = 4;
        res->base_type = base_type;
    }

    return(res);
}

Type *
type_func(Type *ret_type, FuncParam *params)
{
    Type *res;

    res = MALLOC_TYPE(Type);
    if(res)
    {
        res->kind = TYPE_FUNC;
        res->base_type = ret_type;
        res->params = params;
    }

    return(res);
}

/******************************************************************************/
/**                                SYM TABLE                                 **/
/******************************************************************************/

typedef struct
{
    char *id;
    Type *type;
    int global;
    int offset;
    void *func;
} Sym;

#define SYM_TABLE_SIZE 1000
Sym sym_table[SYM_TABLE_SIZE];
int sym_table_count = 0;

void
sym_reset()
{
    sym_table_count = 0;
}

Sym *
sym_add(char *id, Type *type)
{
    Sym *res = 0;

    res = &(sym_table[sym_table_count]);
    ++sym_table_count;
    res->id = id;
    res->type = type;
    res->global = 0;
    res->offset = 0;
    res->func = 0;

    return(res);
}

Sym *
sym_get(char *id)
{
    Sym *res = 0;
    int i;

    for(i = 0;
        i < sym_table_count;
        ++i)
    {
        if(id == sym_table[i].id)
        {
            res = &(sym_table[i]);
            break;
        }
    }

    return(res);
}

void
init_builtin_sym()
{
    /* TODO: This is a type function not int */
    sym_add("putchar", type_int());
}

/******************************************************************************/
/**                                  AST                                     **/
/******************************************************************************/

enum
{
    EXPR_INTLIT,
    EXPR_ID,

    EXPR_CALL,

    EXPR_UNARY,
    EXPR_NEG = EXPR_UNARY,
    EXPR_UNARY_END,

    EXPR_BINARY,
    EXPR_MUL = EXPR_BINARY,
    EXPR_DIV,
    EXPR_MOD,
    EXPR_ADD,
    EXPR_SUB,
    EXPR_ASSIGN,
    EXPR_BINARY_END,

    EXPR_COUNT
};

typedef struct
Expr
{
    int kind;
    int value;
    char *id;
    struct Expr *l;
    struct Expr *r;
    struct Expr *next;
} Expr;

Expr *
dup_expr(Expr *expr)
{
    Expr *new;
    new = DUP_OBJ(Expr, new, expr);
    return(new);
}

Expr *
make_expr_intlit(int value)
{
    Expr *res = 0;

    res = MALLOC_TYPE(Expr);
    if(res)
    {
        res->kind = EXPR_INTLIT;
        res->value = value;
        res->next = 0;
    }

    return(res);
}

Expr *
make_expr_id(char *id)
{
    Expr *res = 0;

    res = MALLOC_TYPE(Expr);
    if(res)
    {
        res->kind = EXPR_ID;
        res->id = id;
        res->next = 0;
    }

    return(res);
}

Expr *
make_expr_unary(int kind, Expr *l)
{
    Expr *res = 0;

    res = MALLOC_TYPE(Expr);
    if(res)
    {
        res->kind = kind;
        res->l = l;
        res->r = 0;
        res->next = 0;
    }

    return(res);
}

Expr *
make_expr_binary(int kind, Expr *l, Expr *r)
{
    Expr *res = 0;

    res = MALLOC_TYPE(Expr);
    if(res)
    {
        res->kind = kind;
        res->l = l;
        res->r = r;
        res->next = 0;
    }

    return(res);
}

void
print_expr(Expr *expr)
{
    char *op = 0;

    switch(expr->kind)
    {
        case EXPR_ID:
        {
            printf("%s", expr->id);
        } break;

        case EXPR_INTLIT:
        {
            printf("%d", expr->value);
        } break;

        case EXPR_CALL:
        {
            printf("call %s", expr->l->id);
        } break;

        case EXPR_MUL: { op = "*"; } break;
        case EXPR_DIV: { op = "/"; } break;
        case EXPR_MOD: { op = "%"; } break;
        case EXPR_ADD: { op = "+"; } break;
        case EXPR_SUB: { op = "-"; } break;

        case EXPR_ASSIGN: { op = "="; } break;

        default:
        {
            fatal("Invalid expression");
        } break;
    }

    if(op)
    {
        printf("(%s ", op);
        print_expr(expr->l);
        printf(" ");
        print_expr(expr->r);
        printf(")");
    }
}

typedef struct
Decl
{
    Type *type;
    char *id;
} Decl;

Decl *
make_decl(Type *type, char *id)
{
    Decl *res = 0;

    res = MALLOC_TYPE(Decl);
    if(res)
    {
        res->type = type;
        res->id = id;
    }

    return(res);
}

void
print_type(Type *type)
{
    if(type == type_void())
    {
        printf("void");
    }
    else if(type == type_char())
    {
        printf("char");
    }
    else if(type == type_int())
    {
        printf("int");
    }
    else if(type->kind == TYPE_PTR)
    {
        printf("ptr to ");
        print_type(type->base_type);
    }
    else
    {
        fatal("Invalid type to print");
    }
}

void
print_decl(Decl *decl)
{
    printf("(var %s ", decl->id);
    print_type(decl->type);
    printf(")");
}

enum
{
    STMT_DECL,
    STMT_EXPR,
    STMT_BLOCK,
    STMT_RET,

    STMT_COUNT
};

typedef struct
Stmt
{
    int kind;
    union
    {
        Decl *decl;
        Expr *expr;
        struct Stmt *block;
    } u;
    struct Stmt *next;
} Stmt;

Stmt *
make_stmt(int kind)
{
    Stmt *res = 0;

    res = MALLOC_TYPE(Stmt);
    if(res)
    {
        res->kind = kind;
        res->next = 0;
    }

    return(res);
}

void
print_stmt(Stmt *stmt)
{
    Stmt *sub_stmt;

    switch(stmt->kind)
    {
        case STMT_DECL:
        {
            print_decl(stmt->u.decl);
        } break;

        case STMT_EXPR:
        {
            print_expr(stmt->u.expr);
        } break;

        case STMT_BLOCK:
        {
            printf("(\n");
            sub_stmt = stmt->u.block;
            while(sub_stmt)
            {
                printf("  ");
                print_stmt(sub_stmt);
                sub_stmt = sub_stmt->next;
            }
            printf(")");
        } break;

        case STMT_RET:
        {
            printf("(ret ");
            if(stmt->u.expr)
            {
                print_expr(stmt->u.expr);
            }
            printf(")");
        } break;

        default:
        {
            fatal("Invalid statement to print");
        } break;
    }

    printf("\n");
}

enum
{
    GLOB_DECL_VAR,
    GLOB_DECL_FUNC,

    GLOB_DECL_COUNT
};

typedef struct
GlobDecl
{
    int kind;
    struct GlobDecl *next;
    char *id;
    Type *type;
    Stmt *func_def;
} GlobDecl;

GlobDecl *
make_glob_decl(int kind)
{
    GlobDecl *res = 0;

    res = MALLOC_TYPE(GlobDecl);
    if(res)
    {
        res->kind = kind;
        res->next = 0;
    }

    return(res);
}

GlobDecl *
make_glob_decl_var(char *id, Type *type)
{
    GlobDecl *res = make_glob_decl(GLOB_DECL_VAR);

    if(res)
    {
        res->id = id;
        res->type = type;
    }

    return(res);
}

GlobDecl *
make_glob_decl_func(char *id, Type *type, Stmt *func_def)
{
    GlobDecl *res = make_glob_decl(GLOB_DECL_FUNC);

    if(res)
    {
        res->id = id;
        res->type = type;
        res->func_def = func_def;
    }

    return(res);
}

void
print_glob_decl(GlobDecl *decl)
{
    switch(decl->kind)
    {
        case GLOB_DECL_VAR:
        {
            printf("(var %s ", decl->id);
            print_type(decl->type);
            printf(")");
        } break;

        case GLOB_DECL_FUNC:
        {
            printf("(func %s ", decl->id);
            print_type(decl->type->base_type);
            if(decl->func_def)
            {
                printf("\n");
                print_stmt(decl->func_def);
            }
            printf(")\n");
        } break;

        default:
        {
            fatal("Invalid global declaration to print");
        } break;
    }

    printf("\n");
}

void
print_unit(GlobDecl *unit)
{
    GlobDecl *curr;

    curr = unit;
    while(curr)
    {
        print_glob_decl(curr);
        curr = curr->next;
    }
}

/******************************************************************************/
/**                                 PARSER                                   **/
/******************************************************************************/

#include <ctype.h>
#include <assert.h>

char *source;
int source_line;

int func_var_offset;

char *kword_int;
char *kword_return;
char *kword_if;

#include <stdarg.h>

void
syntax_error(char *fmt, ...)
{
    va_list ap;

    printf("[!] SYNTAX ERROR: Line %d: ", source_line);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void
syntax_fatal(char *fmt, ...)
{
    va_list ap;

    printf("[!] SYNTAX ERROR: Line %d: ", source_line);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    exit(1);
}

void
parser_init(char *src)
{
    source = src;
    source_line = 1;

    kword_int = str_intern("int");
    kword_return = str_intern("return");
    kword_if = str_intern("if");
}

enum
{
    TOK_EOF,

    TOK_ID,
    TOK_INTLIT,

    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,

    TOK_SEMI,
    TOK_COMMA,

    /* Binary operators */
    TOK_BIN_OP,

    TOK_STAR = TOK_BIN_OP,
    TOK_SLASH,
    TOK_PERCENT,
    TOK_PLUS,
    TOK_MINUS,
    TOK_EQUAL,

    TOK_BIN_OP_END,

    TOK_KW_INT,
    TOK_KW_RETURN,
    TOK_KW_IF,

    TOK_COUNT
};

#define MAX_ID_LEN 33

typedef struct Token
{
    int kind;
    int value;
    char *id;
} Token;

Token
_tok_next(int update_source)
{
    Token tok;
    char *src = source;
    int srcl = source_line;

    if(!*src)
    {
        tok.kind = TOK_EOF;
    }
    else
    {
        while(isspace(*src))
        {
            if(*src == '\n')
            {
                ++srcl;
            }
            ++src;
        }

        switch(*src)
        {
            case '_':
            case 'a':case 'b':case 'c':case 'd':case 'e':case 'f':case 'g':
            case 'h':case 'i':case 'j':case 'k':case 'l':case 'm':case 'n':
            case 'o':case 'p':case 'q':case 'r':case 's':case 't':case 'u':
            case 'v':case 'w':case 'x':case 'y':case 'z':
            case 'A':case 'B':case 'C':case 'D':case 'E':case 'F':case 'G':
            case 'H':case 'I':case 'J':case 'K':case 'L':case 'M':case 'N':
            case 'O':case 'P':case 'Q':case 'R':case 'S':case 'T':case 'U':
            case 'V':case 'W':case 'X':case 'Y':case 'Z':
            {
                char buff[MAX_ID_LEN];
                int buffsize = 0;

                tok.kind = TOK_ID;

                while(isalnum(*src) || *src == '_')
                {
                    buff[buffsize++] = *src;
                    ++src;
                    assert(buffsize < MAX_ID_LEN);
                }
                buff[buffsize] = 0;

                tok.id = str_intern(buff);

                if(tok.id == kword_int) { tok.kind = TOK_KW_INT; }
                else if(tok.id == kword_return) { tok.kind = TOK_KW_RETURN; }
                else if(tok.id == kword_if) { tok.kind = TOK_KW_IF; }
            } break;

            case '0':case '1':case '2':case '3':case '4':
            case '5':case '6':case '7':case '8':case '9':
            {
                tok.kind = TOK_INTLIT;

                /* TODO: Hexadecimal */
                tok.value = 0;
                while(isdigit(*src))
                {
                    tok.value = tok.value*10 + (*src - '0');
                    ++src;
                }
            } break;

            case '(': { ++src; tok.kind = TOK_LPAREN; } break;
            case ')': { ++src; tok.kind = TOK_RPAREN; } break;
            case '{': { ++src; tok.kind = TOK_LBRACE; } break;
            case '}': { ++src; tok.kind = TOK_RBRACE; } break;

            case ';': { ++src; tok.kind = TOK_SEMI; } break;
            case ',': { ++src; tok.kind = TOK_COMMA; } break;

            case '*': { ++src; tok.kind = TOK_STAR; } break;
            case '/': { ++src; tok.kind = TOK_SLASH; } break;
            case '%': { ++src; tok.kind = TOK_PERCENT; } break;
            case '+': { ++src; tok.kind = TOK_PLUS; } break;
            case '-': { ++src; tok.kind = TOK_MINUS; } break;

            case '=': { ++src; tok.kind = TOK_EQUAL; } break;

            default:
            {
                syntax_fatal("Invalid token");
            } break;
        }
    }

    if(update_source)
    {
        source = src;
        source_line = srcl;
    }

    return(tok);
}

Token
tok_peek()
{
    return(_tok_next(0));
}

Token
tok_next()
{
    return(_tok_next(1));
}

Token
tok_expect(int tok_kind)
{
    Token tok;
    tok = tok_next();
    assert(tok.kind == tok_kind);
    return(tok);
}

int
tok_is_type(Token tok)
{
    int is_type = 0;

    switch(tok.kind)
    {
        case TOK_KW_INT:
        {
            is_type = 1;
        } break;

        default:
        {
            is_type = 0;
        } break;
    }

    return(is_type);
}

/*
 * <expr> ::= <expr_binary>
 *
 * <expr_binary> ::= <expr_binary> <bin_op> <expr_unary>
 *                 | <expr_unary>
 *
 * <expr_unary> ::= <un_op> <expr_unary>
 *                | <expr_base>
 *
 * <expr_base> ::= '(' <expr> ')'
 *               | <int_lit>
 *               | <ident>
 *
 * <bin_op> ::= [/ *]
 *            | [-+]
 * <un_op> ::= [-+]
 */

int op_precedence_table[TOK_BIN_OP_END - TOK_BIN_OP] = {
    0, /* TOK_STAR */
    0, /* TOK_SLASH */
    0, /* TOK_PERCENT */
    1, /* TOK_PLUS */
    1, /* TOK_MINUS */
    8, /* TOK_EQUAL */
};

Expr *parse_expr_binary(int precedence);
Expr *parse_expr();

Expr *
parse_expr_base()
{
    Expr *expr = 0;
    Token tok;

    tok = tok_next();
    switch(tok.kind)
    {
        case TOK_INTLIT:
        {
            expr = make_expr_intlit(tok.value);
        } break;

        case TOK_ID:
        {
            expr = make_expr_id(tok.id);
        } break;

        case TOK_LPAREN:
        {
            expr = parse_expr();
            tok_expect(TOK_RPAREN);
        } break;

        default:
        {
            syntax_fatal("Invalid base expression");
        } break;
    }

    return(expr);
}

Expr *
reverse_args_list(Expr *args)
{
    Expr *res;

    res = 0;
    if(!args || !(args->next))
    {
        res = args;
    }
    else
    {
        res = reverse_args_list(args->next);
        args->next->next = args;
        args->next = 0;
    }

    return(res);
}

Expr *
parse_expr_unary_post()
{
    Expr *expr = 0;
    Expr *args = 0;
    Expr *arg = 0;
    int prec;
    Token tok;

    expr = parse_expr_base();

    tok = tok_peek();
    switch(tok.kind)
    {
        case TOK_LPAREN:
        {
            tok_next();
            tok = tok_peek();
            if(tok.kind != TOK_RPAREN)
            {
                do
                {
                    if(tok.kind == TOK_COMMA)
                    {
                        tok_next();
                    }
                    prec = op_precedence_table[TOK_EQUAL];
                    if(arg)
                    {
                        arg->next = parse_expr_binary(prec);
                        arg = arg->next;
                    }
                    else
                    {
                        arg = parse_expr_binary(prec);
                        args = arg;
                    }
                    tok = tok_peek();
                }
                while(tok.kind == TOK_COMMA);
            }
            tok_expect(TOK_RPAREN);

            if(args)
            {
                args = reverse_args_list(args);
            }
            expr = make_expr_binary(EXPR_CALL, expr, args);
        } break;
    }

    return(expr);
}

Expr *
parse_expr_unary()
{
    Expr *expr = 0;
    Token tok;

    tok = tok_peek();
    switch(tok.kind)
    {
        case TOK_MINUS:
        {
            tok_next();
            expr = parse_expr_unary();
            expr = make_expr_unary(EXPR_NEG, expr);
        } break;

        case TOK_PLUS:
        {
            tok_next();
            expr = parse_expr_unary_post();
        } break;

        default:
        {
            expr = parse_expr_unary_post();
        } break;
    }

    return(expr);
}

Expr *
parse_expr_binary(int precedence)
{
    Expr *l = 0;
    Expr *r = 0;
    Token tok;
    int new_precedence;

    l = parse_expr_unary();
    tok = tok_peek();
    while(tok.kind >= TOK_BIN_OP && tok.kind < TOK_BIN_OP_END)
    {
        new_precedence = op_precedence_table[tok.kind - TOK_BIN_OP];
        if(new_precedence < precedence)
        {
            tok_next();
            r = parse_expr_binary(new_precedence);
        }
        else
        {
            break;
        }

        switch(tok.kind)
        {
            case TOK_STAR:    { l = make_expr_binary(EXPR_MUL, l, r); } break;
            case TOK_SLASH:   { l = make_expr_binary(EXPR_DIV, l, r); } break;
            case TOK_PERCENT: { l = make_expr_binary(EXPR_MOD, l, r); } break;
            case TOK_PLUS:    { l = make_expr_binary(EXPR_ADD, l, r); } break;
            case TOK_MINUS:   { l = make_expr_binary(EXPR_SUB, l, r); } break;
            case TOK_EQUAL:   { l = make_expr_binary(EXPR_ASSIGN, l, r); } break;

            default:
            {
                syntax_fatal("Invalid binary expression");
            } break;
        }

        tok = tok_peek();
    }

    return(l);
}

Expr *
parse_expr_assign()
{
    Expr *expr;
    Token tok;

    expr = parse_expr_binary(999);
    tok = tok_peek();
    if(tok.kind == TOK_EQUAL)
    {
        tok_next();
        expr = make_expr_binary(EXPR_ASSIGN, expr, parse_expr_binary(999));
    }

    return(expr);
}

Expr *
parse_expr()
{
    return(parse_expr_assign());
}

/*
 * <decl> ::= <type> <ident> ';'
 */

Type *
get_base_type()
{
    Type *type = 0;
    Token tok;

    tok = tok_peek();
    switch(tok.kind)
    {
        case TOK_KW_INT:
        {
            tok_next();
            type = type_int();
        } break;
    }

    return(type);
}

Type *
parse_type(Type *base_type)
{
    Type *type = 0;
    Token tok;

    type = base_type;

    tok = tok_peek();
    while(tok.kind == TOK_STAR)
    {
        tok_next();
        type = type_ptr(type);
        tok = tok_peek();
    }

    return(type);
}

Decl *
parse_decl()
{
    Decl *decl;
    Type *type;
    Token tok;

    type = get_base_type();
    if(!type)
    {
        syntax_fatal("Invalid type for variable declaration");
    }

    type = parse_type(type);

    tok = tok_expect(TOK_ID);

    decl = make_decl(type, tok.id);

    /* TODO: Parse variable initialization */

    tok_expect(TOK_SEMI);

    return(decl);
}

/*
 * <stmt> ::= <decl>
 *          | <expr>
 *          | <stmt_block>
 */

Stmt *parse_stmt();

Stmt *
parse_stmt_block()
{
    Stmt *stmt;
    Stmt *sub_stmt;
    Token tok;

    tok_expect(TOK_LBRACE);

    stmt = make_stmt(STMT_BLOCK);

    tok = tok_peek();
    sub_stmt = 0;
    while(tok.kind != TOK_RBRACE)
    {
        if(sub_stmt)
        {
            sub_stmt->next = parse_stmt();
            sub_stmt = sub_stmt->next;
        }
        else
        {
            sub_stmt = parse_stmt();
            stmt->u.block = sub_stmt;
        }
        tok = tok_peek();
    }

    tok_expect(TOK_RBRACE);

    return(stmt);
}

Stmt *
parse_stmt()
{
    Stmt *stmt;
    Token tok;

    tok = tok_peek();
    while(tok.kind == TOK_SEMI)
    {
        tok_next();
        tok = tok_peek();
    }

    if(tok_is_type(tok))
    {
        stmt = make_stmt(STMT_DECL);
        stmt->u.decl = parse_decl();
    }
    else
    {
        switch(tok.kind)
        {
            case TOK_LBRACE:
            {
                stmt = parse_stmt_block();
            } break;

            case TOK_KW_RETURN:
            {
                tok_next();
                stmt = make_stmt(STMT_RET);
                stmt->u.expr = 0;
                tok = tok_peek();
                if(tok.kind != TOK_SEMI)
                {
                    stmt->u.expr = parse_expr();
                }
                tok_expect(TOK_SEMI);
            } break;

            default:
            {
                stmt = make_stmt(STMT_EXPR);
                stmt->u.expr = parse_expr();
                tok_expect(TOK_SEMI);
            } break;
        }
    }

    return(stmt);
}

/*
 * <glob_decl> ::= <type> <ident> ['(' <func_params>? ')' [<stmt_block>|';']]?
 * <func_params> ::= <type> <ident>
 */

FuncParam *
parse_func_param()
{
    FuncParam *param;
    char *id;
    Type *type;
    Token tok;

    type = get_base_type();
    if(!type)
    {
        syntax_fatal("Invalid type for function parameter");
    }

    type = parse_type(type);

    tok = tok_expect(TOK_ID);
    id = tok.id;

    param = make_func_param(id, type);

    return(param);
}

GlobDecl *
parse_glob_decl()
{
    GlobDecl *glob_decl = 0;

    Type *type;
    Token tok;
    char *id;
    FuncParam *params;
    FuncParam *curr_param;
    Stmt *func_def;

    type = get_base_type();
    if(!type)
    {
        syntax_fatal("Invalid type for global declaration");
    }

    type = parse_type(type);

    tok = tok_expect(TOK_ID);
    id = tok.id;

    tok = tok_peek();
    if(tok.kind == TOK_SEMI)
    {
        tok_expect(TOK_SEMI);
        glob_decl = make_glob_decl_var(id, type);
    }
    else
    {
        params = 0;

        tok_expect(TOK_LPAREN);
        tok = tok_peek();
        while(tok.kind != TOK_RPAREN)
        {
            if(curr_param)
            {
                curr_param->next = parse_func_param();
                curr_param = curr_param->next;
            }
            else
            {
                curr_param = parse_func_param();
                curr_param->next = 0;
                params = curr_param;
            }

            tok = tok_peek();
            if(tok.kind != TOK_COMMA)
            {
                break;
            }
        }
        tok_expect(TOK_RPAREN);

        type = type_func(type, params);

        func_def = 0;
        tok = tok_peek();
        if(tok.kind == TOK_LBRACE)
        {
            func_def = parse_stmt_block();
        }
        else
        {
            tok_expect(TOK_SEMI);
        }
        glob_decl = make_glob_decl_func(id, type, func_def);
    }

    return(glob_decl);
}

/*
 * <unit> ::= <glob_decl>*
 */

GlobDecl *
parse_unit()
{
    GlobDecl *first = 0;
    GlobDecl *last = 0;
    Token tok;

    tok = tok_peek();
    while(tok.kind != TOK_EOF)
    {
        if(last)
        {
            last->next = parse_glob_decl();
            last = last->next;
        }
        else
        {
            last = parse_glob_decl();
        }

        if(!first)
        {
            first = last;
        }

        tok = tok_peek();
    }

    return(first);
}

/******************************************************************************/
/**                           SEMANTIC ANALYSIS                              **/
/******************************************************************************/

#include <stdarg.h>

void
semantic_error(char *fmt, ...)
{
    va_list ap;

    printf("[!] SEMANTIC ERROR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

void
semantic_fatal(char *fmt, ...)
{
    va_list ap;

    printf("[!] SEMANTIC ERROR: ");
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");

    exit(1);
}

Type *
eval_expr_type(Expr *expr)
{
    Type *type = 0;
    Type *type1;
    Type *type2;
    Sym *sym;

    switch(expr->kind)
    {
        case EXPR_ID:
        {
            sym = sym_get(expr->id);
            if(!sym)
            {
                semantic_fatal("Invalid symbol %s", expr->id);
            }
            type = sym->type;
        } break;

        case EXPR_INTLIT:
        {
            if(expr->value < 256)
            {
                type = type_char();
            }
            else
            {
                type = type_int();
            }
        } break;

        default:
        {
            if(expr->kind == EXPR_CALL)
            {
                if(expr->l->kind != EXPR_ID)
                {
                    semantic_fatal("Invalid function call");
                }

                sym = sym_get(expr->l->id);
                if(!sym)
                {
                    semantic_fatal("Invalid symbol %s", expr->l->id);
                }
                if(!sym->global)
                {
                    semantic_fatal("Invalid function call: %s not a global symbol", sym->id);
                }
                if(sym->type->kind != TYPE_FUNC)
                {
                    semantic_fatal("Invalid function call: %s is not a function", sym->id);
                }

                type = sym->type->base_type;
            }
            else if(expr->kind >= EXPR_UNARY && expr->kind < EXPR_UNARY_END)
            {
                type = eval_expr_type(expr->l);
            }
            else if((expr->kind >= EXPR_BINARY && expr->kind < EXPR_BINARY_END) ||
                    expr->kind == EXPR_ASSIGN)
            {
                type1 = eval_expr_type(expr->l);
                type2 = eval_expr_type(expr->r);
                if(type1 == type_void() || type2 == type_void())
                {
                    semantic_fatal("Invalid expression type (void in expr)");
                }

                if((type1 == type_int() && type2 == type_int()) ||
                   (type1 == type_int() && type2 == type_char()) ||
                   (type1 == type_char() && type2 == type_int()))
                {
                    type = type_int();
                }
                else if(type1 == type_char() && type2 == type_char())
                {
                    type = type_char();
                }

                /* TODO: Pointers */
            }
            else
            {
                semantic_fatal("Invalid expression type");
            }
        } break;
    }

    if(!type)
    {
        semantic_fatal("Invalid expression type (no type)");
    }

    return(type);
}

int
check_lvalue(Expr *expr)
{
    int res = 0;

    if(expr->kind != EXPR_ID)
    {
        semantic_fatal("Invalid lvalue");
    }

    res = 1;
    return(res);
}

int
check_expr(Expr *expr)
{
    int res = 0;
    Sym *sym;

    switch(expr->kind)
    {
        case EXPR_ID:
        {
            sym = sym_get(expr->id);
            if(!sym)
            {
                semantic_fatal("Invalid symbol %s", expr->id);
            }
            res = (eval_expr_type(expr) != 0);
        } break;

        case EXPR_INTLIT:
        {
            res = (eval_expr_type(expr) != 0);
        } break;

        default:
        {
            if(expr->kind == EXPR_CALL)
            {
                if(expr->l->kind != EXPR_ID)
                {
                    semantic_fatal("Invalid function call");
                }

                sym = sym_get(expr->l->id);
                if(!sym->global)
                {
                    semantic_fatal("Invalid function call: %s not a global symbol", sym->id);
                }
                if(sym->type->kind != TYPE_FUNC)
                {
                    semantic_fatal("Invalid function call: %s is not a function", sym->id);
                }

                res = (eval_expr_type(expr) != 0);
            }
            else if(expr->kind >= EXPR_UNARY && expr->kind < EXPR_UNARY_END)
            {
                res = check_expr(expr->l) && eval_expr_type(expr);
            }
            else if(expr->kind >= EXPR_BINARY && expr->kind < EXPR_BINARY_END)
            {
                res = (check_expr(expr->l) &&
                       check_expr(expr->r) &&
                       eval_expr_type(expr) != 0);
            }
            else if(expr->kind == EXPR_ASSIGN)
            {
                res = (check_lvalue(expr->l) &&
                       check_expr(expr->r) &&
                       eval_expr_type(expr) != 0);
            }
            else
            {
                res = 0;
            }
        } break;
    }

    return(res);
}

int
check_stmt(Stmt *stmt)
{
    int res = 0;
    Sym *sym;
    Decl *decl;
    Stmt *curr;

    switch(stmt->kind)
    {
        case STMT_DECL:
        {
            decl = stmt->u.decl;
            sym = sym_get(decl->id);
            if(sym)
            {
                semantic_fatal("Symbol '%s' already declared", decl->id);
            }

            sym = sym_add(decl->id, decl->type);
            sym->global = 0;
            sym->offset = func_var_offset;
            func_var_offset -= decl->type->size;

            res = 1;
        } break;

        case STMT_EXPR:
        {
            res = check_expr(stmt->u.expr);
        } break;

        case STMT_BLOCK:
        {
            res = 1;
            curr = stmt->u.block;
            while(curr)
            {
                res = res && check_stmt(curr);
                curr = curr->next;
            }
        } break;

        default:
        {
            res = 0;
        } break;
    }

    return(res);
}

int
check_glob_decl(GlobDecl *decl)
{
    int res = 0;
    Sym *sym;

    switch(decl->kind)
    {
        case GLOB_DECL_VAR:
        {
            sym = sym_get(decl->id);
            if(sym)
            {
                semantic_fatal("Symbol '%s' already declared", decl->id);
            }

            sym = sym_add(decl->id, decl->type);
            sym->global = 1;

            res = 1;
        } break;

        case GLOB_DECL_FUNC:
        {
            sym = sym_get(decl->id);
            if(sym)
            {
                semantic_fatal("Symbol '%s' already declared", decl->id);
            }

            sym = sym_add(decl->id, decl->type);
            sym->global = 1;

            func_var_offset = -4;
            res = check_stmt(decl->func_def);
        } break;

        default:
        {
            res = 0;
        } break;
    }

    return(res);
}

int
check_unit(GlobDecl *unit)
{
    int res = 1;
    GlobDecl *curr;

    init_builtin_sym();

    curr = unit;
    while(curr)
    {
        res = res && check_glob_decl(curr);
        curr = curr->next;
    }

    return(res);
}

/******************************************************************************/
/**                              AST -> IR-C                                 **/
/******************************************************************************/

/*
 * We use the AST to represent our Intermediate Representation tree since
 * our IR language is a subset of the C language (hence IR-C).
 *
 * IR-C Description:
 *
 * There are only 5 kinds of statement:
 * 1. Return statements
 * 2. Expressions
 * 3. Jumps
 * 4. Labels definition
 * 5. Branches
 *
 * Sources:
 * [1] https://ls12-www.cs.tu-dortmund.de/daes/media/documents/publications/downloads/2003-samosIII.pdf
 */

char tmp_var_buff[64];
int tmp_vars_count = 0;

char *
tmp_var()
{
    int n;

    tmp_var_buff[0] = '_';
    tmp_var_buff[1] = '_';
    tmp_var_buff[2] = '_';
    tmp_var_buff[3] = 't';
    n = sprintf(tmp_var_buff+4, "%d", tmp_vars_count);
    tmp_var_buff[n+4] = 0;
    ++tmp_vars_count;

    return(str_intern(tmp_var_buff));
}

Stmt *curr_block;

void
add_stmt(Stmt *stmt)
{
    Stmt *last;

    last = curr_block->u.block;
    while(last && last->next)
    {
        last = last->next;
    }

    if(last)
    {
        last->next = stmt;
    }
    else
    {
        curr_block->u.block = stmt;
    }
}

Stmt *block_to_irc(Stmt *block);

int
expr_is_atom(Expr *expr)
{
    if(expr->kind == EXPR_INTLIT || expr->kind == EXPR_ID)
    {
        return(1);
    }
    else
    {
        return(0);
    }
}

char *
store_expr_temp_var(Expr *expr, int first)
{
    char *res;
    Stmt *stmt;
    Expr *rvalue;
    Expr *l;
    Expr *r;

    Expr *args;
    Expr *arg;
    Expr *tmp;

    if(!first)
    {
        res = tmp_var();
        stmt = make_stmt(STMT_DECL);
        stmt->u.decl = make_decl(eval_expr_type(expr), res);
        add_stmt(stmt);
    }

    if(expr->kind == EXPR_CALL)
    {
        if(expr_is_atom(expr->l))
        {
            l = expr->l;
        }
        else
        {
            l = make_expr_id(store_expr_temp_var(expr->l, 0));
        }

        args = 0;
        arg = 0;
        r = expr->r;
        while(r)
        {
            tmp = 0;
            if(expr_is_atom(r))
            {
                tmp = dup_expr(r);
            }
            else
            {
                tmp = make_expr_id(store_expr_temp_var(r, 0));
            }

            if(arg)
            {
                arg->next = tmp;
                arg = arg->next;
            }
            else
            {
                arg = tmp;
                args = arg;
            }
            arg->next = 0;

            r = r->next;
        }

        rvalue = make_expr_binary(EXPR_CALL, l, args);
    }
    else if(expr->kind >= EXPR_UNARY && expr->kind < EXPR_UNARY_END)
    {
        if(expr_is_atom(expr->l))
        {
            l = expr->l;
        }
        else
        {
            l = make_expr_id(store_expr_temp_var(expr->l, 0));
        }
        rvalue = make_expr_binary(expr->kind, l, expr->r);
    }
    else if(expr->kind >= EXPR_BINARY && expr->kind < EXPR_BINARY_END)
    {
        if(expr_is_atom(expr->l))
        {
            l = expr->l;
        }
        else
        {
            l = make_expr_id(store_expr_temp_var(expr->l, 0));
        }

        if(expr_is_atom(expr->r))
        {
            r = expr->r;
        }
        else
        {
            r = make_expr_id(store_expr_temp_var(expr->r, 0));
        }

        rvalue = make_expr_binary(expr->kind, l, r);
    }
    else
    {
        assert(0);
    }

    if(!first)
    {
        stmt = make_stmt(STMT_EXPR);
        stmt->u.expr = make_expr_binary(EXPR_ASSIGN, make_expr_id(res), rvalue);
        add_stmt(stmt);
    }
    else
    {
        stmt = make_stmt(STMT_EXPR);
        stmt->u.expr = rvalue;
        add_stmt(stmt);
    }
    

    return(res);
}

void
stmt_to_irc(Stmt *stmt)
{
    Stmt *irc_stmt;
    char *res;
    Sym *sym;

    irc_stmt = 0;
    switch(stmt->kind)
    {
        case STMT_DECL:
        {
            irc_stmt = DUP_OBJ(Stmt, irc_stmt, stmt);
            irc_stmt->next = 0;
            add_stmt(irc_stmt);

            sym = sym_add(stmt->u.decl->id, stmt->u.decl->type);
            sym->global = 0;
            sym->offset = func_var_offset;
            func_var_offset -= stmt->u.decl->type->size;
        } break;

        case STMT_EXPR:
        {
            store_expr_temp_var(stmt->u.expr, 1);
        } break;

        case STMT_BLOCK:
        {
            block_to_irc(stmt);
        } break;

        case STMT_RET:
        {
            irc_stmt = make_stmt(STMT_RET);
            irc_stmt->next = 0;
            if(stmt->u.expr)
            {
                if(expr_is_atom(stmt->u.expr))
                {
                    irc_stmt->u.expr = stmt->u.expr;
                }
                else
                {
                    res = store_expr_temp_var(stmt->u.expr, 0);
                    irc_stmt->u.expr = make_expr_id(res);
                }
            }
            add_stmt(irc_stmt);
        } break;

        default:
        {
            semantic_fatal("invalid statement");
        } break;
    }
}

Stmt *
block_to_irc(Stmt *block)
{
    Stmt *irc_block;
    Stmt *stmt;
    Stmt *parent_block;

    irc_block = make_stmt(STMT_BLOCK);
    irc_block->u.block = 0;
    parent_block = curr_block;
    curr_block = irc_block;
    stmt = block->u.block;
    while(stmt)
    {
        stmt_to_irc(stmt);
        stmt = stmt->next;
    }

    curr_block = parent_block;

    return(irc_block);
}

GlobDecl *
unit_to_irc(GlobDecl *unit)
{
    GlobDecl *irc_unit;
    GlobDecl *irc_curr;
    GlobDecl *glob_decl;
    Sym *sym;

    sym_reset();

    irc_unit = 0;
    irc_curr = 0;
    glob_decl = unit;
    while(glob_decl)
    {
        if(irc_unit)
        {
            irc_curr->next = DUP_OBJ(GlobDecl, irc_curr->next, glob_decl);
            irc_curr = irc_curr->next;
        }
        else
        {
            irc_unit = DUP_OBJ(GlobDecl, irc_unit, glob_decl);
            irc_curr = irc_unit;
        }
        irc_curr->next = 0;

        switch(glob_decl->kind)
        {
            case GLOB_DECL_VAR:
            {
                sym = sym_add(glob_decl->id, glob_decl->type);
                sym->global = 1;
            } break;

            case GLOB_DECL_FUNC:
            {
                if(glob_decl->func_def)
                {
                    irc_curr->func_def = block_to_irc(glob_decl->func_def);
                }
                sym = sym_add(glob_decl->id, glob_decl->type);
                sym->global = 1;
            } break;

            default:
            {
                semantic_fatal("invalid global declaration");
            } break;
        }
        glob_decl = glob_decl->next;
    }

    return(irc_unit);
}

/******************************************************************************/
/**                               CODE GEN                                   **/
/******************************************************************************/

#include <assert.h>

int lbl_count = 0;

void
lbl_gen(char *lbl)
{
    int n;

    lbl[0] = '.';
    lbl[1] = 'L';
    n = sprintf(lbl+2, "%d", lbl_count);
    lbl[n+2] = 0;
    ++lbl_count;
}

void
compile_lvalue(FILE *fout, Expr *expr)
{
    Sym *sym;

    switch(expr->kind)
    {
        case EXPR_ID:
        {
            sym = sym_get(expr->id);
            if(!sym)
            {
                fatal("Invalid symbol %s", expr->id);
            }

            /* TODO: Only ints for now */
            assert(sym->type == type_int());

            if(sym->global)
            {
                fprintf(fout, "\tmovl %s,%%eax\n", sym->id);
            }
            else
            {
                /*fprintf(fout, "\tmovl %d(%%ebp),%%eax\n", sym->offset);*/
                fprintf(fout, "\tmovl %%ebp,%%eax\n");
                fprintf(fout, "\taddl $%d,%%eax\n", sym->offset);
            }
        } break;

        default:
        {
            fatal("Invalid lvalue");
        } break;
    }
}

void
compile_expr(FILE *fout, Expr *expr)
{
    Sym *sym = 0;
    Expr *arg;
    int argc;

    switch(expr->kind)
    {
        case EXPR_INTLIT:
        {
            fprintf(fout, "\tmovl $%d,%%eax\n", expr->value);
        } break;

        case EXPR_ID:
        {
            sym = sym_get(expr->id);
            if(!sym)
            {
                fatal("Invalid symbol %s", expr->id);
            }

            /* TODO: Only ints for now */
            assert(sym->type == type_int());

            if(sym->global)
            {
                fprintf(fout, "\tmovl %s,%%ebx\n", sym->id);
                fprintf(fout, "\tmovl (%%ebx),%%eax\n");
            }
            else
            {
                fprintf(fout, "\tmovl %d(%%ebp),%%eax\n", sym->offset);
            }
        } break;

        case EXPR_CALL:
        {
            if(expr->l->kind == EXPR_ID)
            {
                sym = sym_get(expr->l->id);
                if(!sym)
                {
                    fatal("Invalid symbol %s", expr->l->id);
                }

                assert(sym->type->kind == TYPE_FUNC);

                argc = 0;
                arg = expr->r;
                while(arg)
                {
                    ++argc;
                    compile_expr(fout, arg);
                    fprintf(fout, "\tpushl %%eax\n");
                    arg = arg->next;
                }

                fprintf(fout, "\tcall %s\n", expr->l->id);

                while(argc > 0)
                {
                    fprintf(fout, "\tpopl %%eax\n");
                    --argc;
                }
            }
            else
            {
                fatal("We don't handle \"complex\" function calls");
            }
        } break;

        case EXPR_MUL:
        {
            compile_expr(fout, expr->r);
            fprintf(fout, "\tmovl %%eax,%%ecx\n");
            compile_expr(fout, expr->l);
            fprintf(fout, "\timull %%ecx\n");
        } break;

        case EXPR_DIV:
        {
            compile_expr(fout, expr->r);
            fprintf(fout, "\tmovl %%eax,%%ecx\n");
            compile_expr(fout, expr->l);
            fprintf(fout, "\tidivl %%ecx\n");
        } break;

        case EXPR_ADD:
        {
            compile_expr(fout, expr->r);
            fprintf(fout, "\tmovl %%eax,%%ecx\n");
            compile_expr(fout, expr->l);
            fprintf(fout, "\taddl %%ecx,%%eax\n");
        } break;

        case EXPR_SUB:
        {
            compile_expr(fout, expr->r);
            fprintf(fout, "\tmovl %%eax,%%ecx\n");
            compile_expr(fout, expr->l);
            fprintf(fout, "\tsubl %%ecx,%%eax\n");
        } break;

        case EXPR_ASSIGN:
        {
            compile_expr(fout, expr->r);
            if(0 && expr->l->kind == EXPR_ID)
            {
                sym = sym_get(expr->l->id);
                if(sym->global)
                {
                    fprintf(fout, "\tmovl %%eax,%s\n", expr->l->id);
                }
                else
                {
                    fprintf(fout, "\tmovl %%eax,%d(%%ebp)\n", sym->offset);
                }
            }
            else
            {
                fprintf(fout, "\tmovl %%eax,%%ecx\n");
                compile_lvalue(fout, expr->l);
                fprintf(fout, "\tmovl %%ecx,(%%eax)\n");
            }
        } break;

        default:
        {
            fatal("Invalid expression");
        } break;
    }
}

void
compile_decl(FILE *fout, Decl *decl)
{
    Sym *sym;

    assert(decl->type && decl->type->size > 0);
    fprintf(fout, "\tsubl $%d,%%esp\n", decl->type->size);

    sym = sym_add(decl->id, decl->type);
    sym->global = 0;
    sym->offset = func_var_offset;
    func_var_offset -= decl->type->size;
}

void
compile_stmt(FILE *fout, Stmt *stmt)
{
    int scope;
    Stmt *substmt;

    switch(stmt->kind)
    {
        case STMT_DECL:
        {
            compile_decl(fout, stmt->u.decl);
        } break;

        case STMT_EXPR:
        {
            compile_expr(fout, stmt->u.expr);
        } break;

        case STMT_BLOCK:
        {
            scope = sym_table_count;
            substmt = stmt->u.block;
            while(substmt)
            {
                compile_stmt(fout, substmt);
                substmt = substmt->next;
            }
            sym_table_count = scope;
        } break;

        case STMT_RET:
        {
            if(stmt->u.expr)
            {
                compile_expr(fout, stmt->u.expr);
            }
            fprintf(fout, "\tleave\n");
            fprintf(fout, "\tret\n");
        } break;

        default:
        {
            fatal("Invalid statement");
        } break;
    }
}

void
compile_glob_decl(FILE *fout, GlobDecl *decl)
{
    Sym *sym;

    switch(decl->kind)
    {
        case GLOB_DECL_VAR:
        {
            fprintf(fout, "%s:\n", decl->id);
            fprintf(fout, "\t.zero $%d\n", decl->type->size);

            sym = sym_add(decl->id, decl->type);
            sym->global = 1;
        } break;

        case GLOB_DECL_FUNC:
        {
            func_var_offset = -4;

            if(decl->func_def)
            {
                fprintf(fout, "%s:\n", decl->id);
                fprintf(fout, "\tpushl %%ebp\n");
                fprintf(fout, "\tmovl %%esp,%%ebp\n");
                compile_stmt(fout, decl->func_def);
                fprintf(fout, "\tleave\n");
                fprintf(fout, "\tret\n");
            }

            sym = sym_add(decl->id, decl->type);
            sym->global = 1;
        } break;

        default:
        {
            fatal("Invalid global declaration");
        } break;
    }
}

void
compile_unit(FILE *fout, GlobDecl *unit)
{
    GlobDecl *curr;
#ifdef DEBUG
    FILE *flibc;
    char curr;
#endif

    sym_reset();

    fprintf(fout, "___entry:\n");
    fprintf(fout, "\tpushl %%ebp\n");
    fprintf(fout, "\tmovl %%esp,%%ebp\n");
    fprintf(fout, "\tcall main\n");
    fprintf(fout, "\tmovl %%eax,%%ebx\n");
    fprintf(fout, "\tmovl $1,%%eax\n");
    fprintf(fout, "\tsyscall\n");
    fprintf(fout, "\tleave\n");
    fprintf(fout, "\tret\n");

#ifdef DEBUG
    flibc = fopen("libc.asm", "r");
    if(flibc)
    {
        curr = (char)fgetc(flibc);
        while(curr != EOF)
        {
            fprintf(fout, "%c", curr);
            curr = (char)fgetc(flibc);
        }
        fclose(flibc);
    }

    sym_add(str_intern("putchar"), type_func(type_int(), ));
#else
    /* TODO: Hardcode libc into a C string */
#endif

    curr = unit;
    while(curr)
    {
        compile_glob_decl(fout, curr);
        curr = curr->next;
    }
}

#if 0
#define ASMORG_API
#include "asmorg.c"
#endif

#include <assert.h>

#define PRINT 1

int
main(int argc, char *argv[])
{
    FILE *fout;
    char *src;
    GlobDecl *unit;

    fout = fopen("a.out.asm", "w");

    src = "int globalvar;\n"
          "int putchar(int c);\n"
          "int foo() { return 69; }\n"
          "int main() {\n"
          "    return foo();\n"
          "}";
    parser_init(src);
    unit = parse_unit();
    check_unit(unit);
#if PRINT
    print_unit(unit);
    printf("\n\n+++++++++++++++\nIRC\n+++++++++++++++\n\n");
#endif
    unit = unit_to_irc(unit);
#if PRINT
    print_unit(unit);
    printf("\n\n+++++++++++++++\nx86\n+++++++++++++++\n\n");
#endif
    compile_unit(fout, unit);

    fclose(fout);

    return(0);
}
