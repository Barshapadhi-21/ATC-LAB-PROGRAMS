#include <stdio.h>
#include <string.h>
#include <ctype.h>

// ─── 1. Validate Identifier: starts with letter/_, followed by letters/digits/_ ───
int validateIdentifier(char *str) {
    int i = 0;
    if (str[i] == '\0') return 0;

    if (!isalpha(str[i]) && str[i] != '_') return 0;
    i++;

    while (str[i] != '\0') {
        if (!isalnum(str[i]) && str[i] != '_') return 0;
        i++;
    }
    return 1;
}

// ─── 2. Validate Integer: optional sign, then digits only ───────────────────────
int validateInteger(char *str) {
    int i = 0;
    if (str[i] == '+' || str[i] == '-') i++;
    if (str[i] == '\0') return 0;

    while (str[i] != '\0') {
        if (!isdigit(str[i])) return 0;
        i++;
    }
    return 1;
}

// ─── 3. Validate Float: digits . digits (e.g. 3.14, -2.5) ──────────────────────
int validateFloat(char *str) {
    int i = 0;
    int dotCount = 0;

    if (str[i] == '+' || str[i] == '-') i++;
    if (str[i] == '\0') return 0;

    while (str[i] != '\0') {
        if (str[i] == '.') {
            dotCount++;
            if (dotCount > 1) return 0;
        } else if (!isdigit(str[i])) {
            return 0;
        }
        i++;
    }
    return (dotCount == 1);
}

// ─── 4. Validate Email: name@domain.ext ─────────────────────────────────────────
int validateEmail(char *str) {
    int i = 0;
    int atCount = 0;
    int dotAfterAt = 0;
    int atPos = -1;
    int len = strlen(str);

    if (len == 0) return 0;

    // Count @ symbols
    for (i = 0; i < len; i++) {
        if (str[i] == '@') {
            atCount++;
            atPos = i;
        }
    }
    if (atCount != 1) return 0;       // must have exactly one @
    if (atPos == 0) return 0;          // @ cannot be first
    if (atPos == len - 1) return 0;    // @ cannot be last

    // Check there's a dot after @
    for (i = atPos + 1; i < len; i++) {
        if (str[i] == '.') dotAfterAt = 1;
    }
    if (!dotAfterAt) return 0;

    // Check no spaces anywhere
    for (i = 0; i < len; i++) {
        if (isspace(str[i])) return 0;
    }

    // Check last char is not a dot
    if (str[len - 1] == '.') return 0;

    return 1;
}

// ─── 5. Validate Phone Number: 10 digits only ───────────────────────────────────
int validatePhone(char *str) {
    int i = 0;
    int len = strlen(str);

    if (len != 10) return 0;

    while (str[i] != '\0') {
        if (!isdigit(str[i])) return 0;
        i++;
    }
    return 1;
}

// ─── Print result helper ─────────────────────────────────────────────────────────
void printResult(char *label, char *input, int valid) {
    printf("  %-15s : %-20s => %s\n", label, input, valid ? "VALID ✓" : "INVALID ✗");
}

// ─── Main menu ───────────────────────────────────────────────────────────────────
int main() {
    char input[200];
    int choice;

    printf("==========================================\n");
    printf("    REGULAR EXPRESSION VALIDATOR          \n");
    printf("==========================================\n");

    while (1) {
        printf("\nSelect pattern to validate:\n");
        printf("  1. Identifier  (e.g. my_var, x1)\n");
        printf("  2. Integer     (e.g. 42, -7, +100)\n");
        printf("  3. Float       (e.g. 3.14, -2.5)\n");
        printf("  4. Email       (e.g. abc@gmail.com)\n");
        printf("  5. Phone No.   (e.g. 9876543210)\n");
        printf("  6. Exit\n");
        printf("------------------------------------------\n");
        printf("Enter choice: ");
        scanf("%d", &choice);
        getchar(); // consume newline

        if (choice == 6) {
            printf("Exiting...\n");
            break;
        }

        if (choice < 1 || choice > 5) {
            printf("Invalid choice! Try again.\n");
            continue;
        }

        printf("Enter input to validate: ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0'; // remove newline

        printf("\n------------------------------------------\n");

        switch (choice) {
            case 1: printResult("Identifier", input, validateIdentifier(input)); break;
            case 2: printResult("Integer",    input, validateInteger(input));    break;
            case 3: printResult("Float",      input, validateFloat(input));      break;
            case 4: printResult("Email",      input, validateEmail(input));      break;
            case 5: printResult("Phone No.",  input, validatePhone(input));      break;
        }

        printf("------------------------------------------\n");
    }

    return 0;
}
