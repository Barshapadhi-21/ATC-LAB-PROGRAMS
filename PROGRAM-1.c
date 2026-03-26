#include <stdio.h>
#include <string.h>
#include <ctype.h>

// List of keywords
const char *keywords[] = {
    "int", "float", "double", "char", "if", "else",
    "while", "for", "return", "void", "main"
};
int keyword_count = 11;

// Check if a string is a keyword
int isKeyword(char *word) {
    for (int i = 0; i < keyword_count; i++) {
        if (strcmp(word, keywords[i]) == 0)
            return 1;
    }
    return 0;
}

// Check if a char is an operator
int isOperatorChar(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/' ||
            c == '=' || c == '<' || c == '>' || c == '!' ||
            c == '&' || c == '|' || c == '%');
}

// Check if a char is punctuation
int isPunctuation(char c) {
    return (c == '(' || c == ')' || c == '{' || c == '}' ||
            c == ';' || c == ',' || c == '[' || c == ']');
}

// Print operator name
void printOperatorName(char *op) {
    if      (strcmp(op, "+")  == 0) printf("OPERATOR    : +   (Addition)\n");
    else if (strcmp(op, "-")  == 0) printf("OPERATOR    : -   (Subtraction)\n");
    else if (strcmp(op, "*")  == 0) printf("OPERATOR    : *   (Multiplication)\n");
    else if (strcmp(op, "/")  == 0) printf("OPERATOR    : /   (Division)\n");
    else if (strcmp(op, "%")  == 0) printf("OPERATOR    : %%   (Modulus)\n");
    else if (strcmp(op, "=")  == 0) printf("OPERATOR    : =   (Assignment)\n");
    else if (strcmp(op, "==") == 0) printf("OPERATOR    : ==  (Equal to)\n");
    else if (strcmp(op, "!=") == 0) printf("OPERATOR    : !=  (Not equal)\n");
    else if (strcmp(op, "<")  == 0) printf("OPERATOR    : <   (Less than)\n");
    else if (strcmp(op, ">")  == 0) printf("OPERATOR    : >   (Greater than)\n");
    else if (strcmp(op, "<=") == 0) printf("OPERATOR    : <=  (Less or equal)\n");
    else if (strcmp(op, ">=") == 0) printf("OPERATOR    : >=  (Greater or equal)\n");
    else if (strcmp(op, "&&") == 0) printf("OPERATOR    : &&  (Logical AND)\n");
    else if (strcmp(op, "||") == 0) printf("OPERATOR    : ||  (Logical OR)\n");
    else if (strcmp(op, "!")  == 0) printf("OPERATOR    : !   (Logical NOT)\n");
    else if (strcmp(op, "++") == 0) printf("OPERATOR    : ++  (Increment)\n");
    else if (strcmp(op, "--") == 0) printf("OPERATOR    : --  (Decrement)\n");
    else                            printf("OPERATOR    : %s\n", op);
}

// Main tokenizer function
void tokenize(char *expr) {
    int i = 0;
    int len = strlen(expr);

    while (i < len) {
        // Skip whitespace
        if (isspace(expr[i])) {
            i++;
            continue;
        }

        // Identifier or Keyword: starts with letter or underscore
        if (isalpha(expr[i]) || expr[i] == '_') {
            char word[100];
            int j = 0;
            while (i < len && (isalnum(expr[i]) || expr[i] == '_')) {
                word[j++] = expr[i++];
            }
            word[j] = '\0';

            if (isKeyword(word))
                printf("KEYWORD     : %s\n", word);
            else
                printf("IDENTIFIER  : %s\n", word);
        }

        // Number: integer or float
        else if (isdigit(expr[i])) {
            char num[100];
            int j = 0;
            while (i < len && (isdigit(expr[i]) || expr[i] == '.')) {
                num[j++] = expr[i++];
            }
            num[j] = '\0';
            printf("NUMBER      : %s\n", num);
        }

        // Operators (handle 2-char operators first)
        else if (isOperatorChar(expr[i])) {
            char op[3] = {0};
            op[0] = expr[i];

            // Check for 2-character operators
            if (i + 1 < len && isOperatorChar(expr[i + 1])) {
                op[1] = expr[i + 1];
                // Valid 2-char operators
                if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
                    strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0 ||
                    strcmp(op, "&&") == 0 || strcmp(op, "||") == 0 ||
                    strcmp(op, "++") == 0 || strcmp(op, "--") == 0) {
                    printOperatorName(op);
                    i += 2;
                    continue;
                }
                op[1] = '\0'; // reset, it's a single-char operator
            }

            op[0] = expr[i];
            op[1] = '\0';
            printOperatorName(op);
            i++;
        }

        // Punctuation
        else if (isPunctuation(expr[i])) {
            switch (expr[i]) {
                case '(': printf("PUNCTUATION : (   (Left Parenthesis)\n"); break;
                case ')': printf("PUNCTUATION : )   (Right Parenthesis)\n"); break;
                case '{': printf("PUNCTUATION : {   (Left Brace)\n"); break;
                case '}': printf("PUNCTUATION : }   (Right Brace)\n"); break;
                case '[': printf("PUNCTUATION : [   (Left Bracket)\n"); break;
                case ']': printf("PUNCTUATION : ]   (Right Bracket)\n"); break;
                case ';': printf("PUNCTUATION : ;   (Semicolon)\n"); break;
                case ',': printf("PUNCTUATION : ,   (Comma)\n"); break;
            }
            i++;
        }

        // Unknown character
        else {
            printf("UNKNOWN     : %c\n", expr[i]);
            i++;
        }
    }
}

int main() {
    char expr[500];

    printf("========================================\n");
    printf("   LEXICAL ANALYZER - Token Identifier  \n");
    printf("========================================\n");
    printf("Enter an expression: ");
    fgets(expr, sizeof(expr), stdin);

    // Remove trailing newline
    expr[strcspn(expr, "\n")] = '\0';

    printf("\n--- Tokens Identified ---\n");
    tokenize(expr);
    printf("-------------------------\n");
    printf("Analysis complete.\n");

    return 0;
}
