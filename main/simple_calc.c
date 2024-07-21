#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: simple_calc [num1] [operation] [num2]\n");
        return 1;
    }

    double num1 = atof(argv[1]);
    double num2 = atof(argv[3]);
    char operation = argv[2][0];
    double result;

    switch (operation) {
        case '+':
            result = num1 + num2;
            break;
        case '-':
            result = num1 - num2;
            break;
        case '*':
            result = num1 * num2;
            break;
        case '/':
            if (num2 == 0) {
                printf("Error: Division by zero\n");
                return 1;
            }
            result = num1 / num2;
            break;
        default:
            printf("Error: Invalid operation\n");
            return 1;
    }

    printf("Result: %f\n", result);
    return 0;
}
