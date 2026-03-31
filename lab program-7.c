/*
 * ============================================================
 *  Program 7 – AST Construction  (Pure-C, no tools needed)
 *
 *  Compile : gcc -o ast_standalone ast_standalone.c
 *  Run     : ./ast_standalone
 *
 *  Supports: assignment, +  -  *  /  ()  integers  variables
 *  Grammar :
 *    stmt   → ID '=' expr ';'  |  expr ';'
 *    expr   → term   (('+' | '-') term)*
 *    term   → factor (('*' | '/') factor)*
 *    factor → NUMBER | ID | '(' expr ')'
 * ============================================================
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ── colours ──────────────────────────────────────────────── */
#define GRN  "\033[1;32m"
#define CYN  "\033[1;36m"
#define YLW  "\033[1;33m"
#define MAG  "\033[1;35m"
#define RED  "\033[1;31m"
#define RST  "\033[0m"

/* ══════════════════════════════════════════════════════════ *
 *  LEXER
 * ══════════════════════════════════════════════════════════ */
typedef enum {
    T_NUM, T_ID,
    T_PLUS, T_MINUS, T_MUL, T_DIV,
    T_LPAREN, T_RPAREN, T_ASSIGN, T_SEMI,
    T_EOF, T_ERR
} TokType;

typedef struct { TokType type; int ival; char sval[64]; } Token;

static const char *src;
static int         pos;

static Token next_token(void) {
    Token t = {0};
    while (src[pos] && isspace((unsigned char)src[pos])) pos++;
    if (!src[pos]) { t.type = T_EOF; return t; }

    char c = src[pos];

    if (isdigit((unsigned char)c)) {
        t.type = T_NUM; t.ival = 0;
        while (isdigit((unsigned char)src[pos])) t.ival = t.ival*10 + (src[pos++]-'0');
        return t;
    }
    if (isalpha((unsigned char)c) || c == '_') {
        int i = 0;
        while (isalnum((unsigned char)src[pos]) || src[pos]=='_') t.sval[i++] = src[pos++];
        t.sval[i] = '\0'; t.type = T_ID; return t;
    }
    pos++;
    switch (c) {
        case '+': t.type = T_PLUS;   break;
        case '-': t.type = T_MINUS;  break;
        case '*': t.type = T_MUL;    break;
        case '/': t.type = T_DIV;    break;
        case '(': t.type = T_LPAREN; break;
        case ')': t.type = T_RPAREN; break;
        case '=': t.type = T_ASSIGN; break;
        case ';': t.type = T_SEMI;   break;
        default:  t.type = T_ERR;    break;
    }
    return t;
}

/* peek / consume helpers */
static Token cur;
static void  advance(void)          { cur = next_token(); }
static int   peek(TokType t)        { return cur.type == t; }
static Token consume(TokType t)     {
    if (cur.type != t) { fprintf(stderr, RED "Expected token %d, got %d\n" RST, t, cur.type); exit(1); }
    Token tmp = cur; advance(); return tmp;
}

/* ══════════════════════════════════════════════════════════ *
 *  AST
 * ══════════════════════════════════════════════════════════ */
typedef enum { N_NUM, N_ID, N_BINOP, N_ASSIGN } NType;

typedef struct Node {
    NType        type;
    char         op;
    int          ival;
    char         sval[64];
    struct Node *left, *right;
} Node;

static Node *mknum(int v)  { Node *n=calloc(1,sizeof*n); n->type=N_NUM;   n->ival=v; return n; }
static Node *mkid(const char *s) { Node *n=calloc(1,sizeof*n); n->type=N_ID; strncpy(n->sval,s,63); return n; }
static Node *mkbin(char op, Node *l, Node *r) { Node *n=calloc(1,sizeof*n); n->type=N_BINOP; n->op=op; n->left=l; n->right=r; return n; }
static Node *mkassign(const char *id, Node *e) { Node *n=calloc(1,sizeof*n); n->type=N_ASSIGN; strncpy(n->sval,id,63); n->right=e; return n; }

/* ── print ─────────────────────────────────────────────────── */
static const char *BRANCHES[] = {"    ", "│   ", "├── ", "└── "};
void print_ast(Node *n, const char *prefix, int is_last) {
    if (!n) return;
    printf("%s%s", prefix, is_last ? BRANCHES[3] : BRANCHES[2]);

    char new_prefix[256];
    snprintf(new_prefix, sizeof new_prefix, "%s%s", prefix, is_last ? BRANCHES[0] : BRANCHES[1]);

    switch (n->type) {
    case N_NUM:
        printf(GRN "NUM" RST "(%d)\n", n->ival);
        break;
    case N_ID:
        printf(CYN "ID" RST "(%s)\n", n->sval);
        break;
    case N_BINOP:
        printf(YLW "BINOP" RST "('%c')\n", n->op);
        print_ast(n->left,  new_prefix, 0);
        print_ast(n->right, new_prefix, 1);
        break;
    case N_ASSIGN:
        printf(MAG "ASSIGN" RST "\n");
        /* print LHS as a leaf */
        printf("%s%s" CYN "ID" RST "(%s)  ← LHS\n",
               new_prefix, BRANCHES[2], n->sval);
        print_ast(n->right, new_prefix, 1);
        break;
    }
}

static void free_ast(Node *n) {
    if (!n) return;
    free_ast(n->left); free_ast(n->right); free(n);
}

/* ══════════════════════════════════════════════════════════ *
 *  PARSER  (recursive-descent)
 * ══════════════════════════════════════════════════════════ */
static Node *parse_expr(void);   /* forward */

static Node *parse_factor(void) {
    if (peek(T_NUM))    { Token t=consume(T_NUM);    return mknum(t.ival); }
    if (peek(T_ID))     { Token t=consume(T_ID);     return mkid(t.sval); }
    if (peek(T_LPAREN)) {
        consume(T_LPAREN);
        Node *n = parse_expr();
        consume(T_RPAREN);
        return n;
    }
    fprintf(stderr, RED "Unexpected token in factor\n" RST); exit(1);
}

static Node *parse_term(void) {
    Node *n = parse_factor();
    while (peek(T_MUL) || peek(T_DIV)) {
        char op = peek(T_MUL) ? '*' : '/';
        advance();
        n = mkbin(op, n, parse_factor());
    }
    return n;
}

static Node *parse_expr(void) {
    Node *n = parse_term();
    while (peek(T_PLUS) || peek(T_MINUS)) {
        char op = peek(T_PLUS) ? '+' : '-';
        advance();
        n = mkbin(op, n, parse_term());
    }
    return n;
}

static Node *parse_stmt(void) {
    /* look-ahead: ID '=' → assignment */
    if (peek(T_ID)) {
        char id[64];
        strncpy(id, cur.sval, 63);
        Token saved = cur;
        advance();
        if (peek(T_ASSIGN)) {
            advance();                  /* consume '=' */
            Node *e = parse_expr();
            consume(T_SEMI);
            return mkassign(id, e);
        }
        /* not an assignment — treat saved as start of expr */
        /* put the ID back as a leaf and continue */
        Node *left = mkid(id);
        while (peek(T_PLUS)||peek(T_MINUS)||peek(T_MUL)||peek(T_DIV)) {
            char op = peek(T_PLUS)?'+':peek(T_MINUS)?'-':peek(T_MUL)?'*':'/';
            advance();
            left = mkbin(op, left, parse_factor());
        }
        consume(T_SEMI);
        return left;
    }
    Node *e = parse_expr();
    consume(T_SEMI);
    return e;
}

/* ══════════════════════════════════════════════════════════ *
 *  DEMO INPUTS
 * ══════════════════════════════════════════════════════════ */
static void run(const char *input) {
    printf(CYN "\n──────────────────────────────────────\n" RST);
    printf("Input : %s\n", input);
    src = input; pos = 0; advance();
    Node *tree = parse_stmt();
    printf("AST   :\n");
    print_ast(tree, "", 1);
    free_ast(tree);
}

int main(void) {
    printf(MAG "╔══════════════════════════════════════╗\n");
    printf(    "║  Program 7 : AST Construction (C)   ║\n");
    printf(    "╚══════════════════════════════════════╝\n" RST);

    run("3 + 4 * 2;");
    run("x = a + b * c - d;");
    run("result = (a + b) * (c - d);");
    run("y = 10 / 2 + 3 * (4 - 1);");
    run("z = x + y;");

    /* interactive mode */
    printf(CYN "\n──────────────────────────────────────\n");
    printf("Enter your own expressions (end with ';')\n");
    printf("Type 'quit;' to exit.\n" RST);
    char buf[512];
    while (1) {
        printf(YLW ">> " RST);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (strncmp(buf, "quit", 4) == 0) break;
        if (strchr(buf, ';') == NULL) { strcat(buf, ";"); }
        run(buf);
    }
    return 0;
}
