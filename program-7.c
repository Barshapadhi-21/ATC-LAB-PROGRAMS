#include <stdio.h>
#include <string.h>

struct Item {
    char name[50];
    int price;
    char news[100];
    float sentiment;
    char recommendation[100];
    int isUrgent;
};

void showItem(struct Item item) {
    printf("\n============================\n");
    printf("Product: %s\n", item.name);
    printf("Price: $%d\n", item.price);
    printf("News: %s\n", item.news);
    printf("Sentiment Score: %.2f\n", item.sentiment);

    if(item.isUrgent) {
        printf("⚠️ ALERT: Conflict Impact Detected\n");
    }

    printf("Recommendation: %s\n", item.recommendation);

    if(item.isUrgent) {
        printf(">>> BUY NOW <<<\n");
    } else {
        printf(">>> ADD TO CART <<<\n");
    }
    printf("============================\n");
}

int main() {
    int choice;

    struct Item laptop = {
        "Laptop",
        800,
        "Tech market stable",
        0.75,
        "Good time to buy",
        0
    };

    struct Item shirt = {
        "Shirt",
        40,
        "Supply shortage due to conflict",
        0.30,
        "Prices may rise soon",
        1
    };

    printf("Select Item:\n");
    printf("1. Laptop\n");
    printf("2. Shirt\n");
    printf("Enter choice: ");
    scanf("%d", &choice);

    if(choice == 1) {
        showItem(laptop);
    } else if(choice == 2) {
        showItem(shirt);
    } else {
        printf("Invalid choice!\n");
    }

    getchar(); // pause
    getchar();
    return 0;
}
