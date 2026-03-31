/*
 * ================================================================
 *   CODE GENERATION PROGRAM — Simple Expression Assembly
 * ================================================================
 *  Pipeline:
 *    Source Expression
 *       → Lexer  (tokens)
 *       → Parser (AST via recursive descent)
 *       → Code Generator (register-based assembly)
 *
 *  Supports:
 *    • Arithmetic  : +  -  *  /  %  unary-minus
 *    • Comparison  : ==  !=  <  >  <=  >=
 *    • Logical     : &&  ||  !
 *    • Parentheses : full precedence & associativity
 *    • Variables   : single identifiers
 *    • Integers    : numeric literals
 *
 *  Assembly output uses a simple load/store register model:
 *    LOAD   Ri, var/const
 *    MOV    Ri, Rj
 *    ADD    Ri, Rj
 *    SUB    Ri, Rj
 *    MUL    Ri, Rj
 *    DIV    Ri, Rj
 *    MOD    Ri, Rj
 *    CMP_EQ / CMP_NE / CMP_LT / CMP_GT / CMP_LE / CMP_GE
 *    AND    Ri, Rj
 *    OR     Ri, Rj
 *    NOT    Ri
 *    NEG    Ri
 *    STORE  result, Ri
 * ================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ───────────────────────────────── Constants ─────────────────────────── */
#define MAX_TOKENS    512
#define MAX_CODE      1024
#define MAX_REGS      32
#define MAX_NAME      64
#define MAX_LINE      128

/* ═══════════════════════════════ LEXER ══════════════════════════════════ */

typedef enum {
    T_INT, T_IDENT,
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_EQEQ, T_NEQ, T_LT, T_GT, T_LE, T_GE,
    T_AND, T_OR, T_NOT,
    T_LPAREN, T_RPAREN,
    T_EOF, T_UNKNOWN
} TokKind;

static const char *tok_names[] = {
    "INT","IDENT",
    "+","-","*","/","%",
    "==","!=","<",">","<=",">=",
    "&&","||","!",
    "(",")",
    "EOF","UNKNOWN"
};

typedef struct { TokKind kind; char text[MAX_NAME]; int ival; } Token;

static Token  g_tokens[MAX_TOKENS];
static int    g_ntok  = 0;
static int    g_tcur  = 0;   /* current token index */

/* ── tokenise a null-terminated expression string ── */
static void lex(const char *src) {
    g_ntok = 0; g_tcur = 0;
    int i = 0;
    while (src[i]) {
        /* skip whitespace */
        while (src[i] && isspace((unsigned char)src[i])) i++;
        if (!src[i]) break;

        Token t; memset(&t, 0, sizeof t);

        /* number */
        if (isdigit((unsigned char)src[i])) {
            int j = 0;
            while (isdigit((unsigned char)src[i])) t.text[j++] = src[i++];
            t.text[j] = '\0'; t.kind = T_INT; t.ival = atoi(t.text);
        }
        /* identifier */
        else if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            int j = 0;
            while (isalnum((unsigned char)src[i]) || src[i] == '_')
                t.text[j++] = src[i++];
            t.text[j] = '\0'; t.kind = T_IDENT;
        }
        /* two-char operators */
        else if (src[i]=='=' && src[i+1]=='=') { strcpy(t.text,"=="); t.kind=T_EQEQ;   i+=2; }
        else if (src[i]=='!' && src[i+1]=='=') { strcpy(t.text,"!="); t.kind=T_NEQ;    i+=2; }
        else if (src[i]=='<' && src[i+1]=='=') { strcpy(t.text,"<="); t.kind=T_LE;     i+=2; }
        else if (src[i]=='>' && src[i+1]=='=') { strcpy(t.text,">="); t.kind=T_GE;     i+=2; }
        else if (src[i]=='&' && src[i+1]=='&') { strcpy(t.text,"&&"); t.kind=T_AND;    i+=2; }
        else if (src[i]=='|' && src[i+1]=='|') { strcpy(t.text,"||"); t.kind=T_OR;     i+=2; }
        /* single-char */
        else {
            t.text[0] = src[i]; t.text[1] = '\0';
            switch (src[i]) {
                case '+': t.kind=T_PLUS;    break;
                case '-': t.kind=T_MINUS;   break;
                case '*': t.kind=T_STAR;    break;
                case '/': t.kind=T_SLASH;   break;
                case '%': t.kind=T_PERCENT; break;
                case '<': t.kind=T_LT;      break;
                case '>': t.kind=T_GT;      break;
                case '!': t.kind=T_NOT;     break;
                case '(': t.kind=T_LPAREN;  break;
                case ')': t.kind=T_RPAREN;  break;
                default:  t.kind=T_UNKNOWN; break;
            }
            i++;
        }
        if (g_ntok < MAX_TOKENS) g_tokens[g_ntok++] = t;
    }
    Token eof; memset(&eof,0,sizeof eof);
    eof.kind = T_EOF; strcpy(eof.text,"EOF");
    g_tokens[g_ntok++] = eof;
}

/* ── token stream helpers ── */
static Token  peek(void)        { return g_tokens[g_tcur]; }
static Token  advance(void)     { Token t=g_tokens[g_tcur]; if(g_tcur<g_ntok-1) g_tcur++; return t; }
static int    check(TokKind k)  { return peek().kind == k; }
static int    match(TokKind k)  { if(check(k)){advance();return 1;} return 0; }

/* ════════════════════════════════ AST ════════════════════════════════════ */

typedef enum {
    N_INT, N_VAR,
    N_ADD, N_SUB, N_MUL, N_DIV, N_MOD,
    N_EQ,  N_NEQ, N_LT,  N_GT,  N_LE,  N_GE,
    N_AND, N_OR,  N_NOT, N_NEG
} NodeKind;

typedef struct ASTNode {
    NodeKind kind;
    int      ival;
    char     name[MAX_NAME];
    struct ASTNode *left, *right;  /* right==NULL for unary */
} ASTNode;

static ASTNode node_pool[MAX_TOKENS * 4];
static int     node_cnt = 0;

static ASTNode *alloc_node(NodeKind k) {
    ASTNode *n = &node_pool[node_cnt++];
    memset(n, 0, sizeof *n);
    n->kind = k; return n;
}

/* ─────────────── Recursive-descent parser ───────────────────────────── */
/* Precedence (low → high):
   || → && → == != → < > <= >= → + - → * / % → unary → primary */

static ASTNode *parse_expr(void);

static ASTNode *parse_primary(void) {
    if (check(T_INT)) {
        Token t = advance();
        ASTNode *n = alloc_node(N_INT);
        n->ival = t.ival; return n;
    }
    if (check(T_IDENT)) {
        Token t = advance();
        ASTNode *n = alloc_node(N_VAR);
        strncpy(n->name, t.text, MAX_NAME-1); return n;
    }
    if (match(T_LPAREN)) {
        ASTNode *n = parse_expr();
        if (!match(T_RPAREN)) {
            fprintf(stderr, "Error: missing ')'\n"); exit(1);
        }
        return n;
    }
    fprintf(stderr, "Error: unexpected token '%s'\n", peek().text);
    exit(1);
}

static ASTNode *parse_unary(void) {
    if (match(T_MINUS)) {
        ASTNode *n = alloc_node(N_NEG);
        n->left = parse_unary(); return n;
    }
    if (match(T_NOT)) {
        ASTNode *n = alloc_node(N_NOT);
        n->left = parse_unary(); return n;
    }
    return parse_primary();
}

static ASTNode *parse_muldiv(void) {
    ASTNode *left = parse_unary();
    while (check(T_STAR) || check(T_SLASH) || check(T_PERCENT)) {
        TokKind op = peek().kind; advance();
        ASTNode *n = alloc_node(op==T_STAR ? N_MUL : op==T_SLASH ? N_DIV : N_MOD);
        n->left = left; n->right = parse_unary(); left = n;
    }
    return left;
}

static ASTNode *parse_addsub(void) {
    ASTNode *left = parse_muldiv();
    while (check(T_PLUS) || check(T_MINUS)) {
        TokKind op = peek().kind; advance();
        ASTNode *n = alloc_node(op==T_PLUS ? N_ADD : N_SUB);
        n->left = left; n->right = parse_muldiv(); left = n;
    }
    return left;
}

static ASTNode *parse_relational(void) {
    ASTNode *left = parse_addsub();
    while (check(T_LT)||check(T_GT)||check(T_LE)||check(T_GE)) {
        TokKind op = peek().kind; advance();
        NodeKind nk = op==T_LT ? N_LT : op==T_GT ? N_GT :
                      op==T_LE ? N_LE : N_GE;
        ASTNode *n = alloc_node(nk);
        n->left = left; n->right = parse_addsub(); left = n;
    }
    return left;
}

static ASTNode *parse_equality(void) {
    ASTNode *left = parse_relational();
    while (check(T_EQEQ)||check(T_NEQ)) {
        TokKind op = peek().kind; advance();
        ASTNode *n = alloc_node(op==T_EQEQ ? N_EQ : N_NEQ);
        n->left = left; n->right = parse_relational(); left = n;
    }
    return left;
}

static ASTNode *parse_and(void) {
    ASTNode *left = parse_equality();
    while (match(T_AND)) {
        ASTNode *n = alloc_node(N_AND);
        n->left = left; n->right = parse_equality(); left = n;
    }
    return left;
}

static ASTNode *parse_expr(void) {
    ASTNode *left = parse_and();
    while (match(T_OR)) {
        ASTNode *n = alloc_node(N_OR);
        n->left = left; n->right = parse_and(); left = n;
    }
    return left;
}

/* ═══════════════════════════ AST PRINTER ════════════════════════════════ */

static const char *node_kind_name(NodeKind k) {
    switch(k){
        case N_INT: return "INT";    case N_VAR: return "VAR";
        case N_ADD: return "ADD";    case N_SUB: return "SUB";
        case N_MUL: return "MUL";    case N_DIV: return "DIV";
        case N_MOD: return "MOD";    case N_EQ:  return "EQ";
        case N_NEQ: return "NEQ";    case N_LT:  return "LT";
        case N_GT:  return "GT";     case N_LE:  return "LE";
        case N_GE:  return "GE";     case N_AND: return "AND";
        case N_OR:  return "OR";     case N_NOT: return "NOT";
        case N_NEG: return "NEG";
        default:    return "?";
    }
}

static void print_ast(const ASTNode *n, int depth, const char *prefix) {
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");
    if (depth > 0) printf("%s", prefix);
    printf("[%s", node_kind_name(n->kind));
    if (n->kind == N_INT)  printf(": %d", n->ival);
    if (n->kind == N_VAR)  printf(": %s", n->name);
    printf("]\n");
    if (n->left)  print_ast(n->left,  depth+1, "L─ ");
    if (n->right) print_ast(n->right, depth+1, "R─ ");
}

/* ═══════════════════════════ CODE GENERATOR ═════════════════════════════ */

/* ── Register allocator ── */
static int reg_used[MAX_REGS];


static int alloc_reg(void) {
    for (int i = 0; i < MAX_REGS; i++) {
        if (!reg_used[i]) { reg_used[i] = 1; return i; }
    }
    fprintf(stderr, "Register spill: out of registers!\n");
    exit(1);
}

static void free_reg(int r) {
    if (r >= 0) reg_used[r] = 0;
}

/* ── Instruction buffer ── */
static char  code_buf[MAX_CODE][MAX_LINE];
static int   ncode = 0;


static void emit(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(code_buf[ncode], MAX_LINE, fmt, ap);
    va_end(ap);
    ncode++;
}

/* ── Register name helper ── */
static const char *rname(int r) {
    static char buf[16][12];
    static int  ri = 0;
    ri = (ri + 1) % 16;
    snprintf(buf[ri], 12, "R%d", r);
    return buf[ri];
}

/* ── Recursive code generation ──
   Returns the register number that holds the result. */
static int codegen(const ASTNode *n) {
    if (!n) return -1;

    /* ── Leaf: integer constant ── */
    if (n->kind == N_INT) {
        int r = alloc_reg();
        emit("  LOAD   %s, #%d", rname(r), n->ival);
        return r;
    }

    /* ── Leaf: variable ── */
    if (n->kind == N_VAR) {
        int r = alloc_reg();
        emit("  LOAD   %s, %s", rname(r), n->name);
        return r;
    }

    /* ── Unary: NEG / NOT ── */
    if (n->kind == N_NEG || n->kind == N_NOT) {
        int r = codegen(n->left);
        emit("  %-6s %s", n->kind == N_NEG ? "NEG" : "NOT", rname(r));
        return r;
    }

    /* ── Binary ── */
    int rl = codegen(n->left);
    int rr = codegen(n->right);

    /* choose instruction mnemonic */
    const char *mnem = NULL;
    switch (n->kind) {
        case N_ADD: mnem = "ADD";    break;
        case N_SUB: mnem = "SUB";    break;
        case N_MUL: mnem = "MUL";    break;
        case N_DIV: mnem = "DIV";    break;
        case N_MOD: mnem = "MOD";    break;
        case N_EQ:  mnem = "CMP_EQ"; break;
        case N_NEQ: mnem = "CMP_NE"; break;
        case N_LT:  mnem = "CMP_LT"; break;
        case N_GT:  mnem = "CMP_GT"; break;
        case N_LE:  mnem = "CMP_LE"; break;
        case N_GE:  mnem = "CMP_GE"; break;
        case N_AND: mnem = "AND";    break;
        case N_OR:  mnem = "OR";     break;
        default:    mnem = "???";    break;
    }

    emit("  %-6s %s, %s", mnem, rname(rl), rname(rr));
    free_reg(rr);   /* result lives in rl */
    return rl;
}

/* ═════════════════════════════ THREE-ADDRESS CODE ═══════════════════════ */
/* Also generates TAC for comparison */

static int tac_temp = 0;
static char tac_buf[MAX_CODE][MAX_LINE];
static int  ntac = 0;

static void tac_emit(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    vsnprintf(tac_buf[ntac], MAX_LINE, fmt, ap);
    va_end(ap);
    ntac++;
}

/* returns temp name that holds result */
static char *tac_gen(const ASTNode *n, char *out, int outsz) {
    static char pool[64][MAX_NAME];
    static int  pi = 0;
    char *tmp = pool[pi = (pi+1)%64];

    if (n->kind == N_INT) {
        snprintf(out, outsz, "%d", n->ival);
        return out;
    }
    if (n->kind == N_VAR) {
        snprintf(out, outsz, "%s", n->name);
        return out;
    }

    snprintf(tmp, MAX_NAME, "t%d", tac_temp++);

    if (n->kind == N_NEG || n->kind == N_NOT) {
        char la[MAX_NAME];
        tac_gen(n->left, la, MAX_NAME);
        const char *op = n->kind == N_NEG ? "-" : "!";
        tac_emit("  %-6s = %s %s", tmp, op, la);
        strncpy(out, tmp, outsz-1); return out;
    }

    char la[MAX_NAME], ra[MAX_NAME];
    tac_gen(n->left,  la, MAX_NAME);
    tac_gen(n->right, ra, MAX_NAME);

    const char *op = "?";
    switch (n->kind) {
        case N_ADD: op="+";  break; case N_SUB: op="-";  break;
        case N_MUL: op="*";  break; case N_DIV: op="/";  break;
        case N_MOD: op="%";  break; case N_EQ:  op="=="; break;
        case N_NEQ: op="!="; break; case N_LT:  op="<";  break;
        case N_GT:  op=">";  break; case N_LE:  op="<="; break;
        case N_GE:  op=">="; break; case N_AND: op="&&"; break;
        case N_OR:  op="||"; break; default: break;
    }
    tac_emit("  %-6s = %s %s %s", tmp, la, op, ra);
    strncpy(out, tmp, outsz-1);
    return out;
}

/* ═════════════════════════════════ RUNNER ═══════════════════════════════ */

static void sep(char c, int n) { for(int i=0;i<n;i++) putchar(c); putchar('\n'); }

static void run(const char *title, const char *expr, const char *result_var) {
    /* reset everything */
    node_cnt = 0; g_ntok = 0; g_tcur = 0;
    ncode = 0; ntac = 0; tac_temp = 0;
    memset(reg_used, 0, sizeof reg_used);
    memset(code_buf, 0, sizeof code_buf);
    memset(tac_buf,  0, sizeof tac_buf);

    printf("\n");
    sep('=', 64);
    printf("  EXPRESSION : %s = %s\n", result_var, expr);
    printf("  TEST       : %s\n", title);
    sep('-', 64);

    /* ── 1. Lex ── */
    lex(expr);
    printf("  Tokens (%d):\n    ", g_ntok - 1);
    for (int i = 0; i < g_ntok - 1; i++)
        printf("[%s:%s] ", tok_names[g_tokens[i].kind], g_tokens[i].text);
    printf("\n");
    sep('-', 64);

    /* ── 2. Parse ── */
    ASTNode *root = parse_expr();
    printf("  Abstract Syntax Tree:\n");
    print_ast(root, 1, "");
    sep('-', 64);

    /* ── 3. TAC ── */
    char res_name[MAX_NAME];
    tac_gen(root, res_name, MAX_NAME);
    if (strcmp(res_name, result_var) != 0)
        tac_emit("  %-6s = %s", result_var, res_name);

    printf("  Three-Address Code (TAC):\n");
    for (int i = 0; i < ntac; i++) printf("%s\n", tac_buf[i]);
    sep('-', 64);

    /* ── 4. Assembly ── */
    int result_reg = codegen(root);
    emit("  STORE  %s, %s", result_var, rname(result_reg));
    free_reg(result_reg);

    printf("  Assembly Code:\n");
    printf("  ; ---- start: %s ----\n", title);
    for (int i = 0; i < ncode; i++) printf("%s\n", code_buf[i]);
    printf("  ; ---- end ----\n");
    sep('-', 64);

    printf("  Summary:\n");
    printf("    TAC instructions  : %d\n", ntac);
    printf("    Assembly lines    : %d\n", ncode);
    printf("    Result stored in  : %s\n", result_var);
    sep('=', 64);
}

/* ════════════════════════════════ MAIN ══════════════════════════════════ */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║     CODE GENERATION — Simple Expression Assembly             ║\n");
    printf("║  Pipeline: Lexer → Parser (AST) → TAC → Assembly            ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    /* ── Test 1: Simple arithmetic ── */
    run("Simple Addition", "a + b", "result");

    /* ── Test 2: Mixed arithmetic ── */
    run("Mixed Arithmetic", "a + b * c", "x");

    /* ── Test 3: Parenthesised expression ── */
    run("Parentheses Override", "(a + b) * c", "y");

    /* ── Test 4: Subtraction & Division ── */
    run("Sub & Div", "a - b / c + d", "z");

    /* ── Test 5: Comparison ── */
    run("Comparison", "a + b > c - d", "flag");

    /* ── Test 6: Logical ── */
    run("Logical AND/OR", "(a > b) && (c < d)", "cond");

    /* ── Test 7: Unary minus ── */
    run("Unary Minus", "-a + b * -c", "val");

    /* ── Test 8: Logical NOT ── */
    run("Logical NOT", "!(a == b)", "notflag");

    /* ── Test 9: Constants only (folding demo) ── */
    run("All Constants", "3 + 5 * 2 - 4 / 2", "k");

    /* ── Test 10: Complex nested ── */
    run("Complex Nested", "(a + b) * (c - d) / (e + 1)", "ans");

    /* ── Interactive mode ── */
    printf("\n");
    sep('=', 64);
    printf("  INTERACTIVE MODE — enter your own expression (or 'quit')\n");
    printf("  Format:  result = expression\n");
    printf("  Example: sum = a + b * c\n");
    sep('=', 64);

    char line[256];
    while (1) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(line, sizeof line, stdin)) break;
        line[strcspn(line, "\n")] = '\0';
        if (!strcmp(line,"quit") || !strcmp(line,"exit")) break;
        if (!line[0]) continue;

        /* split "result = expr" */
        char *eq = strchr(line, '=');
        if (!eq) { printf("  Use format:  result = expression\n"); continue; }
        *eq = '\0';
        char *lhs = line; char *rhs = eq + 1;
        while(isspace((unsigned char)*lhs)) lhs++;
        int ll = strlen(lhs); while(ll>0 && isspace((unsigned char)lhs[ll-1])) lhs[--ll]='\0';
        while(isspace((unsigned char)*rhs)) rhs++;

        if (!lhs[0] || !rhs[0]) {
            printf("  Invalid input.\n"); continue;
        }
        run("User Input", rhs, lhs);
    }

    printf("\nCode generator terminated.\n");
    return 0;
}
