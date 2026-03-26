#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

char input[200];
int  len;

// Token array
char tokens[100];
int  tlen;

// Tokenize: extract only operators and 'a' (id)
void tokenize() {
    tlen = 0;
    for (int i = 0; i < len; i++) {
        if (input[i] == 'a' || input[i] == '+' || input[i] == '*')
            tokens[tlen++] = input[i];
    }
}

// Count parse trees for tokens[l..r]
// Grammar (ambiguous): E -> E+E | E*E | id
int countParseTrees(int l, int r) {
    if (l == r) {
        // base case: single identifier
        return (tokens[l] == 'a') ? 1 : 0;
    }

    int count = 0;

    // Try every operator position as root
    for (int k = l + 1; k < r; k += 2) {
        char op = tokens[k];
        if (op == '+' || op == '*') {
            int left  = countParseTrees(l, k - 1);
            int right = countParseTrees(k + 1, r);
            count += left * right;
        }
    }
    return count;
}

int main() {
    printf("==========================================\n");
    printf("   PROGRAM 6: GRAMMAR AMBIGUITY CHECKER   \n");
    printf("==========================================\n");
    printf("Grammar: E -> E+E | E*E | id\n");
    printf("This grammar is AMBIGUOUS for expressions\n");
    printf("with mixed operators.\n");
    printf("------------------------------------------\n");

    while (1) {
        printf("\nEnter expression (use 'a' as id, e.g. a+a*a)\n");
        printf("or 'exit': ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) break;

        len = strlen(input);
        tokenize();

        printf("\nTokens: ");
        for (int i = 0; i < tlen; i++) printf("%c ", tokens[i]);
        printf("\n");

        int trees = countParseTrees(0, tlen - 1);

        printf("\n------------------------------------------\n");
        printf("  Number of Parse Trees : %d\n", trees);
        if (trees > 1)
            printf("  Result  : AMBIGUOUS GRAMMAR ✗\n");
        else if (trees == 1)
            printf("  Result  : UNAMBIGUOUS (unique parse) ✓\n");
        else
            printf("  Result  : INVALID expression\n");
        printf("------------------------------------------\n");

        if (trees > 1) {
            printf("\n  Why ambiguous?\n");
            printf("  Expression '%s' has %d different\n", input, trees);
            printf("  parse trees because the grammar has\n");
            printf("  no precedence/associativity rules.\n");
            printf("\n  Example for a+a*a:\n");
            printf("  Tree 1: (a+a)*a  => * is root\n");
            printf("  Tree 2: a+(a*a)  => + is root\n");
        }
    }

    return 0;
}c
