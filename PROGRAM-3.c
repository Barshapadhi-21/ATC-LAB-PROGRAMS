#include <stdio.h>
#include <string.h>
#include <ctype.h>

void countLWC(char *text) {
    int lines = 0;
    int words = 0;
    int chars = 0;

    int i = 0;
    int len = strlen(text);
    int inWord = 0; // flag to track if we are inside a word

    while (i < len) {
        chars++; // count every character

        // Count lines
        if (text[i] == '\n') {
            lines++;
            inWord = 0; // reset word flag at newline
        }

        // Count words (transition from non-space to space)
        if (isspace(text[i])) {
            inWord = 0;
        } else {
            if (inWord == 0) {
                words++;   // new word starts
                inWord = 1;
            }
        }

        i++;
    }

    // If text doesn't end with newline, count last line
    if (len > 0 && text[len - 1] != '\n') {
        lines++;
    }

    printf("\n==========================================\n");
    printf("         WORD COUNT RESULTS (wc)          \n");
    printf("==========================================\n");
    printf("  Lines      : %d\n", lines);
    printf("  Words      : %d\n", words);
    printf("  Characters : %d\n", chars);
    printf("==========================================\n");
}

int main() {
    char text[5000];
    char line[500];

    printf("==========================================\n");
    printf("   LINE / WORD / CHARACTER COUNTER        \n");
    printf("==========================================\n");
    printf("Enter text (type END on a new line to finish):\n");
    printf("------------------------------------------\n");

    text[0] = '\0';

    while (1) {
        fgets(line, sizeof(line), stdin);
        if (strcmp(line, "END\n") == 0 || strcmp(line, "END") == 0)
            break;
        strcat(text, line);
    }

    countLWC(text);

    return 0;
}
