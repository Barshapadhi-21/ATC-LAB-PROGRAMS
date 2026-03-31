/*
 * Control Statements Parser in C
 * Supports: if, if-else, while, for, do-while, switch-case,
 *           break, continue, return
 *
 * Approach: Recursive Descent Parser
 *   - Lexer  : tokenizes the input source string
 *   - Parser : builds a simple AST and pretty-prints it
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ───────────────────────────── Token Types ───────────────────────────── */

typedef enum {
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_FOR, TOK_DO,
    TOK_SWITCH, TOK_CASE, TOK_DEFAULT, TOK_BREAK,
    TOK_CONTINUE, TOK_RETURN,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_SEMICOLON, TOK_COLON,
    TOK_IDENT,       /* any identifier / expression token */
    TOK_NUMBER,
    TOK_OTHER,       /* operators, commas, etc. (treated as expression text) */
    TOK_EOF
} TokenType;

static const char *tok_name[] = {
    "IF","ELSE","WHILE","FOR","DO",
    "SWITCH","CASE","DEFAULT","BREAK",
    "CONTINUE","RETURN",
    "LPAREN","RPAREN","LBRACE","RBRACE",
    "SEMICOLON","COLON","IDENT","NUMBER","OTHER","EOF"
};

typedef struct {
    TokenType type;
    char      lexeme[256];
} Token;

/* ───────────────────────────── Lexer ─────────────────────────────────── */

static const char *src;
static int         pos;

static Token make_tok(TokenType t, const char *lex) {
    Token tk;
    tk.type = t;
    strncpy(tk.lexeme, lex, 255);
    tk.lexeme[255] = '\0';
    return tk;
}

static Token next_token(void) {
    /* Skip whitespace & comments */
    while (src[pos] && isspace((unsigned char)src[pos])) pos++;
    if (!src[pos]) return make_tok(TOK_EOF, "EOF");

    /* Single-line comment */
    if (src[pos] == '/' && src[pos+1] == '/') {
        while (src[pos] && src[pos] != '\n') pos++;
        return next_token();
    }

    /* Single-char tokens */
    char c = src[pos];
    if (c == '(') { pos++; return make_tok(TOK_LPAREN,    "("); }
    if (c == ')') { pos++; return make_tok(TOK_RPAREN,    ")"); }
    if (c == '{') { pos++; return make_tok(TOK_LBRACE,    "{"); }
    if (c == '}') { pos++; return make_tok(TOK_RBRACE,    "}"); }
    if (c == ';') { pos++; return make_tok(TOK_SEMICOLON, ";"); }
    if (c == ':') { pos++; return make_tok(TOK_COLON,     ":"); }

    /* Numbers */
    if (isdigit((unsigned char)c)) {
        char buf[64]; int i = 0;
        while (src[pos] && isdigit((unsigned char)src[pos]))
            buf[i++] = src[pos++];
        buf[i] = '\0';
        return make_tok(TOK_NUMBER, buf);
    }

    /* Identifiers / keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        char buf[256]; int i = 0;
        while (src[pos] && (isalnum((unsigned char)src[pos]) || src[pos] == '_'))
            buf[i++] = src[pos++];
        buf[i] = '\0';

        if (!strcmp(buf,"if"))       return make_tok(TOK_IF,       buf);
        if (!strcmp(buf,"else"))     return make_tok(TOK_ELSE,     buf);
        if (!strcmp(buf,"while"))    return make_tok(TOK_WHILE,    buf);
        if (!strcmp(buf,"for"))      return make_tok(TOK_FOR,      buf);
        if (!strcmp(buf,"do"))       return make_tok(TOK_DO,       buf);
        if (!strcmp(buf,"switch"))   return make_tok(TOK_SWITCH,   buf);
        if (!strcmp(buf,"case"))     return make_tok(TOK_CASE,     buf);
        if (!strcmp(buf,"default"))  return make_tok(TOK_DEFAULT,  buf);
        if (!strcmp(buf,"break"))    return make_tok(TOK_BREAK,    buf);
        if (!strcmp(buf,"continue")) return make_tok(TOK_CONTINUE, buf);
        if (!strcmp(buf,"return"))   return make_tok(TOK_RETURN,   buf);
        return make_tok(TOK_IDENT, buf);
    }

    /* Everything else (operators, etc.) */
    char buf[3] = { c, '\0', '\0' };
    pos++;
    return make_tok(TOK_OTHER, buf);
}

/* ───────────────── Token stream (lookahead buffer) ─────────────────────*/

#define MAX_TOKENS 4096
static Token tokens[MAX_TOKENS];
static int   ntokens = 0;
static int   cur     = 0;          /* current index into tokens[] */

static void tokenize(const char *source) {
    src = source; pos = 0; ntokens = 0; cur = 0;
    do {
        tokens[ntokens] = next_token();
    } while (tokens[ntokens++].type != TOK_EOF && ntokens < MAX_TOKENS - 1);
}

static Token peek(void)  { return tokens[cur]; }
static Token advance(void) {
    Token t = tokens[cur];
    if (cur < ntokens - 1) cur++;
    return t;
}
static int check(TokenType t)  { return peek().type == t; }
static int match(TokenType t)  { if (check(t)) { advance(); return 1; } return 0; }
static void expect(TokenType t) {
    if (!match(t)) {
        fprintf(stderr, "Parse error: expected '%s', got '%s' ('%s')\n",
                tok_name[t], tok_name[peek().type], peek().lexeme);
        exit(1);
    }
}

/* ───────────────────── Pretty-print helpers ────────────────────────── */

static int indent = 0;

static void print_indent(void) {
    for (int i = 0; i < indent; i++) printf("  ");
}

static void print_node(const char *label, const char *extra) {
    print_indent();
    if (extra && extra[0])
        printf("[%s: \"%s\"]\n", label, extra);
    else
        printf("[%s]\n", label);
}

/* ─────────────────────── Forward declarations ───────────────────────── */

static void parse_statement(void);
static void parse_block(void);

/* ─────────────── Expression collector (balanced parens) ──────────────*/
/*
 * Collects tokens up to the first unbalanced ')' or ';' or ':' and
 * returns them as a single string (used for condition / init / update).
 */
static void collect_expr(char *out, int max_len, TokenType stop1, TokenType stop2) {
    int depth = 0, len = 0;
    out[0] = '\0';
    while (1) {
        Token t = peek();
        if (t.type == TOK_EOF) break;
        if (depth == 0 && (t.type == stop1 || (stop2 != TOK_EOF && t.type == stop2))) break;
        if (t.type == TOK_LPAREN) depth++;
        if (t.type == TOK_RPAREN) { if (depth == 0) break; depth--; }
        advance();
        int slen = (int)strlen(t.lexeme);
        if (len + slen + 2 < max_len) {
            if (len > 0) out[len++] = ' ';
            strcpy(out + len, t.lexeme);
            len += slen;
        }
    }
    out[len] = '\0';
}

/* ──────────────────────── Statement parsers ─────────────────────────── */

/* if ( cond ) stmt [ else stmt ] */
static void parse_if(void) {
    char cond[512];
    expect(TOK_IF);
    expect(TOK_LPAREN);
    collect_expr(cond, sizeof cond, TOK_RPAREN, TOK_EOF);
    expect(TOK_RPAREN);

    print_node("IF_STMT", cond);
    indent++;
    print_node("THEN", "");
    indent++;
    parse_statement();
    indent--;

    if (check(TOK_ELSE)) {
        advance();
        print_node("ELSE", "");
        indent++;
        parse_statement();
        indent--;
    }
    indent--;
}

/* while ( cond ) stmt */
static void parse_while(void) {
    char cond[512];
    expect(TOK_WHILE);
    expect(TOK_LPAREN);
    collect_expr(cond, sizeof cond, TOK_RPAREN, TOK_EOF);
    expect(TOK_RPAREN);

    print_node("WHILE_STMT", cond);
    indent++;
    parse_statement();
    indent--;
}

/* do stmt while ( cond ) ; */
static void parse_do_while(void) {
    char cond[512];
    expect(TOK_DO);
    print_node("DO_WHILE_STMT", "");
    indent++;
    print_node("BODY", "");
    indent++;
    parse_statement();
    indent--;
    expect(TOK_WHILE);
    expect(TOK_LPAREN);
    collect_expr(cond, sizeof cond, TOK_RPAREN, TOK_EOF);
    expect(TOK_RPAREN);
    expect(TOK_SEMICOLON);
    print_node("CONDITION", cond);
    indent--;
}

/* for ( init ; cond ; update ) stmt */
static void parse_for(void) {
    char init[256], cond[256], upd[256];
    expect(TOK_FOR);
    expect(TOK_LPAREN);
    collect_expr(init, sizeof init, TOK_SEMICOLON, TOK_EOF);
    expect(TOK_SEMICOLON);
    collect_expr(cond, sizeof cond, TOK_SEMICOLON, TOK_EOF);
    expect(TOK_SEMICOLON);
    collect_expr(upd,  sizeof upd,  TOK_RPAREN,    TOK_EOF);
    expect(TOK_RPAREN);

    print_node("FOR_STMT", "");
    indent++;
    print_node("INIT",      init);
    print_node("CONDITION", cond);
    print_node("UPDATE",    upd);
    print_node("BODY",      "");
    indent++;
    parse_statement();
    indent--;
    indent--;
}

/* switch ( expr ) { case / default / stmts } */
static void parse_switch(void) {
    char expr[512];
    expect(TOK_SWITCH);
    expect(TOK_LPAREN);
    collect_expr(expr, sizeof expr, TOK_RPAREN, TOK_EOF);
    expect(TOK_RPAREN);

    print_node("SWITCH_STMT", expr);
    indent++;
    expect(TOK_LBRACE);

    while (!check(TOK_RBRACE) && !check(TOK_EOF)) {
        if (check(TOK_CASE)) {
            advance();
            char val[256];
            collect_expr(val, sizeof val, TOK_COLON, TOK_EOF);
            expect(TOK_COLON);
            print_node("CASE", val);
            indent++;
            /* parse statements until next case/default/} */
            while (!check(TOK_CASE) && !check(TOK_DEFAULT)
                   && !check(TOK_RBRACE) && !check(TOK_EOF)) {
                parse_statement();
            }
            indent--;
        } else if (check(TOK_DEFAULT)) {
            advance();
            expect(TOK_COLON);
            print_node("DEFAULT", "");
            indent++;
            while (!check(TOK_CASE) && !check(TOK_DEFAULT)
                   && !check(TOK_RBRACE) && !check(TOK_EOF)) {
                parse_statement();
            }
            indent--;
        } else {
            parse_statement();
        }
    }
    expect(TOK_RBRACE);
    indent--;
}

/* break ; */
static void parse_break(void) {
    expect(TOK_BREAK);
    expect(TOK_SEMICOLON);
    print_node("BREAK_STMT", "");
}

/* continue ; */
static void parse_continue(void) {
    expect(TOK_CONTINUE);
    expect(TOK_SEMICOLON);
    print_node("CONTINUE_STMT", "");
}

/* return [expr] ; */
static void parse_return(void) {
    char val[512] = "";
    expect(TOK_RETURN);
    collect_expr(val, sizeof val, TOK_SEMICOLON, TOK_EOF);
    expect(TOK_SEMICOLON);
    print_node("RETURN_STMT", val);
}

/* { stmt* } */
static void parse_block(void) {
    expect(TOK_LBRACE);
    print_node("BLOCK", "");
    indent++;
    while (!check(TOK_RBRACE) && !check(TOK_EOF))
        parse_statement();
    expect(TOK_RBRACE);
    indent--;
}

/* expression-statement: expr ; */
static void parse_expr_stmt(void) {
    char expr[512];
    collect_expr(expr, sizeof expr, TOK_SEMICOLON, TOK_EOF);
    expect(TOK_SEMICOLON);
    if (expr[0]) print_node("EXPR_STMT", expr);
}

/* dispatch */
static void parse_statement(void) {
    switch (peek().type) {
        case TOK_IF:       parse_if();       break;
        case TOK_WHILE:    parse_while();    break;
        case TOK_DO:       parse_do_while(); break;
        case TOK_FOR:      parse_for();      break;
        case TOK_SWITCH:   parse_switch();   break;
        case TOK_BREAK:    parse_break();    break;
        case TOK_CONTINUE: parse_continue(); break;
        case TOK_RETURN:   parse_return();   break;
        case TOK_LBRACE:   parse_block();    break;
        case TOK_SEMICOLON: advance(); break;  /* empty statement */
        default:           parse_expr_stmt(); break;
    }
}

/* ──────────────── Top-level parser: parse a list of statements ────────*/

static void parse_program(void) {
    print_node("PROGRAM", "");
    indent++;
    while (!check(TOK_EOF))
        parse_statement();
    indent--;
}

/* ─────────────────────────── Test cases ─────────────────────────────── */

static void run_test(const char *title, const char *code) {
    printf("\n");
    printf("══════════════════════════════════════════════════════════\n");
    printf("  TEST: %s\n", title);
    printf("──────────────────────────────────────────────────────────\n");
    printf("  Source:\n");
    /* print source with indentation */
    const char *p = code;
    printf("    ");
    while (*p) {
        putchar(*p);
        if (*p == '\n' && *(p+1)) printf("    ");
        p++;
    }
    printf("\n");
    printf("──────────────────────────────────────────────────────────\n");
    printf("  Parse Tree:\n");
    tokenize(code);
    indent = 2;
    parse_program();
    printf("══════════════════════════════════════════════════════════\n");
}

/* ─────────────────────────────── main ───────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║       CONTROL STATEMENTS PARSER  (Recursive Descent)    ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    /* ── Test 1: if-else ── */
    run_test("if-else",
        "if (x > 0) {\n"
        "    result = x;\n"
        "} else {\n"
        "    result = -x;\n"
        "}"
    );

    /* ── Test 2: nested if-else if ── */
    run_test("nested if-else if",
        "if (grade >= 90) {\n"
        "    letter = A;\n"
        "} else if (grade >= 80) {\n"
        "    letter = B;\n"
        "} else {\n"
        "    letter = C;\n"
        "}"
    );

    /* ── Test 3: while loop ── */
    run_test("while loop",
        "while (i < 10) {\n"
        "    sum = sum + i;\n"
        "    i = i + 1;\n"
        "}"
    );

    /* ── Test 4: do-while loop ── */
    run_test("do-while loop",
        "do {\n"
        "    count = count + 1;\n"
        "} while (count < 5);"
    );

    /* ── Test 5: for loop ── */
    run_test("for loop",
        "for (i = 0 ; i < n ; i = i + 1) {\n"
        "    arr[i] = 0;\n"
        "}"
    );

    /* ── Test 6: switch-case ── */
    run_test("switch-case",
        "switch (op) {\n"
        "    case 1:\n"
        "        x = x + y;\n"
        "        break;\n"
        "    case 2:\n"
        "        x = x - y;\n"
        "        break;\n"
        "    default:\n"
        "        x = 0;\n"
        "        break;\n"
        "}"
    );

    /* ── Test 7: break & continue inside for ── */
    run_test("break & continue",
        "for (i = 0 ; i < 100 ; i = i + 1) {\n"
        "    if (i == 50) {\n"
        "        break;\n"
        "    }\n"
        "    if (i % 2 == 0) {\n"
        "        continue;\n"
        "    }\n"
        "    sum = sum + i;\n"
        "}"
    );

    /* ── Test 8: return statement ── */
    run_test("return statement",
        "if (n == 0) {\n"
        "    return 1;\n"
        "}\n"
        "return n * factorial ( n - 1 ) ;"
    );

    /* ── Test 9: interactive mode ── */
    printf("\n");
    printf("══════════════════════════════════════════════════════════\n");
    printf("  INTERACTIVE MODE  (type 'quit' to exit)\n");
    printf("  Enter a control statement to parse:\n");
    printf("══════════════════════════════════════════════════════════\n");

    char buf[4096];
    while (1) {
        printf("\n> ");
        fflush(stdout);
        if (!fgets(buf, sizeof buf, stdin)) break;
        buf[strcspn(buf, "\n")] = '\0';
        if (!strcmp(buf, "quit") || !strcmp(buf, "exit")) break;
        if (!buf[0]) continue;
        tokenize(buf);
        printf("\nParse Tree:\n");
        indent = 1;
        parse_program();
    }

    printf("\nParser terminated.\n");
    return 0;
}
