#include <stdio.h>
#include <string.h>

void removeComments(char *code) {
    int i = 0;
    int len = strlen(code);

    while (i < len) {

        // Handle single-line comment  //
        if (code[i] == '/' && code[i+1] == '/') {
            i += 2;
            while (i < len && code[i] != '\n') {
                i++; // skip till end of line
            }
        }

        // Handle multi-line comment  /* ... */
        else if (code[i] == '/' && code[i+1] == '*') {
            i += 2;
            while (i < len) {
                if (code[i] == '*' && code[i+1] == '/') {
                    i += 2; // skip closing */
                    break;
                }
                if (code[i] == '\n')
                    printf("\n"); // preserve line structure
                i++;
            }
        }

        // Handle string literals (don't remove // or /* inside strings)
        else if (code[i] == '"') {
            printf("%c", code[i++]);
            while (i < len && code[i] != '"') {
                if (code[i] == '\\') {
                    printf("%c", code[i++]); // print escape char
                }
                printf("%c", code[i++]);
            }
            printf("%c", code[i++]); // closing "
        }

        // Print everything else as-is
        else {
            printf("%c", code[i++]);
        }
    }
}

int main() {
    char code[5000];
    char line[500];

    printf("==========================================\n");
    printf("   COMMENT REMOVER FOR C PROGRAMS         \n");
    printf("==========================================\n");
    printf("Enter C code (type END on a new line to finish):\n");
    printf("------------------------------------------\n");

    code[0] = '\0';

    while (1) {
        fgets(line, sizeof(line), stdin);
        if (strcmp(line, "END\n") == 0 || strcmp(line, "END") == 0)
            break;
        strcat(code, line);
    }

    printf("\n==========================================\n");
    printf("   OUTPUT (Comments Removed):             \n");
    printf("==========================================\n");
    removeComments(code);
    printf("\n==========================================\n");
    printf("Done.\n");

    return 0;
}
