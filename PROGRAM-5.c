#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ─────────────────────────────────────────
//   LEXER (Tokenizer)
// ─────────────────────────────────────────

#define TOK_NUM   1
#define TOK_PLUS  2
#define TOK_MINUS 3
#define TOK_MUL   4
#define TOK_DIV   5
#define TOK_MOD   6
#define TOK_LPAREN 7
#define TOK_RPAREN 8
#define TOK_END   9
#define TOK_ERR   0

typedef struct {
    int   type;
    float value; // used if type == TOK_NUM
} Token;

// Global lexer state
char  *src;
int    pos;
Token  currentToken;

// Read next token from input
Token nextToken() {
    Token tok;

    // Skip whitespace
    while (src[pos] != '\0' && isspace(src[pos])) pos++;

    if (src[pos] == '\0') { tok.type = TOK_END; return tok; }

    // Number (integer or float)
    if (isdigit(src[pos]) || (src[pos] == '.' && isdigit(src[pos+1]))) {
        char buf[64]; int j = 0;
        while (isdigit(src[pos]) || src[pos] == '.') buf[j++] = src[pos++];
        buf[j] = '\0';
        tok.type  = TOK_NUM;
        tok.value = atof(buf);
        printf("  [LEXER]  NUMBER     : %s\n", buf);
        return tok;
    }

    switch (src[pos]) {
        case '+': tok.type = TOK_PLUS;   printf("  [LEXER]  OPERATOR   : +\n"); break;
        case '-': tok.type = TOK_MINUS;  printf("  [LEXER]  OPERATOR   : -\n"); break;
        case '*': tok.type = TOK_MUL;    printf("  [LEXER]  OPERATOR   : *\n"); break;
        case '/': tok.type = TOK_DIV;    printf("  [LEXER]  OPERATOR   : /\n"); break;
        case '%': tok.type = TOK_MOD;    printf("  [LEXER]  OPERATOR   : %%\n"); break;
        case '(': tok.type = TOK_LPAREN; printf("  [LEXER]  PUNCTUATION: (\n"); break;
        case ')': tok.type = TOK_RPAREN; printf("  [LEXER]  PUNCTUATION: )\n"); break;
        default:
            printf("  [LEXER]  UNKNOWN    : %c\n", src[pos]);
            tok.type = TOK_ERR;
    }
    pos++;
    return tok;
}

void advance() {
    currentToken = nextToken();
}

// ─────────────────────────────────────────
//   PARSER + EVALUATOR (Recursive Descent)
//   Grammar:
//     expr   → term   { (+|-) term   }
//     term   → factor { (*|/|%) factor }
//     factor → NUMBER | '(' expr ')'  | '-' factor
// ─────────────────────────────────────────

float parseExpr();   // forward declaration
float parseTerm();
float parseFactor();

float parseFactor() {
    float result = 0;

    // Unary minus
    if (currentToken.type == TOK_MINUS) {
        advance();
        return -parseFactor();
    }

    // Parenthesised expression
    if (currentToken.type == TOK_LPAREN) {
        advance(); // consume '('
        result = parseExpr();
        if (currentToken.type == TOK_RPAREN)
            advance(); // consume ')'
        else
            printf("  [ERROR]  Missing closing ')'\n");
        return result;
    }

    // Number
    if (currentToken.type == TOK_NUM) {
        result = currentToken.value;
        advance();
        return result;
    }

    printf("  [ERROR]  Unexpected token\n");
    return 0;
}

float parseTerm() {
    float result = parseFactor();

    while (currentToken.type == TOK_MUL  ||
           currentToken.type == TOK_DIV  ||
           currentToken.type == TOK_MOD) {

        int op = currentToken.type;
        advance();
        float right = parseFactor();

        if (op == TOK_MUL) {
            result *= right;
        } else if (op == TOK_DIV) {
            if (right == 0) { printf("  [ERROR]  Division by zero!\n"); return 0; }
            result /= right;
        } else if (op == TOK_MOD) {
            if ((int)right == 0) { printf("  [ERROR]  Modulo by zero!\n"); return 0; }
            result = (int)result % (int)right;
        }
    }
    return result;
}

float parseExpr() {
    float result = parseTerm();

    while (currentToken.type == TOK_PLUS ||
           currentToken.type == TOK_MINUS) {

        int op = currentToken.type;
        advance();
        float right = parseTerm();

        if (op == TOK_PLUS)  result += right;
        else                 result -= right;
    }
    return result;
}

// ─────────────────────────────────────────
//   MAIN
// ─────────────────────────────────────────

int main() {
    char expr[500];

    printf("==========================================\n");
    printf("  ARITHMETIC EXPRESSION EVALUATOR         \n");
    printf("  (with Lex Tokenizer + YACC-style Parser)\n");
    printf("==========================================\n");

    while (1) {
        printf("\nEnter expression (or 'exit'): ");
        fgets(expr, sizeof(expr), stdin);
        expr[strcspn(expr, "\n")] = '\0';

        if (strcmp(expr, "exit") == 0) {
            printf("Exiting...\n");
            break;
        }

        if (strlen(expr) == 0) continue;

        printf("\n--- Lexer Tokens ---\n");

        // Init lexer
        src = expr;
        pos = 0;
        advance(); // load first token

        printf("\n--- Parser Evaluating ---\n");
        float result = parseExpr();

        printf("\n==========================================\n");
        // Print as integer if result is whole number
        if (result == (int)result)
            printf("  Result : %s = %d\n", expr, (int)result);
        else
            printf("  Result : %s = %.4f\n", expr, result);
        printf("==========================================\n");
    }

    return 0;
}
