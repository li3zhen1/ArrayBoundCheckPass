#include <stdio.h>
int main() {
    // int i = 7;
    // if (i > 8) {
    //     i += 9;
    // } else {
    //     i += 10;
    // }

    int arr[4] = {1, 2, 3, 4};

    for (int i = 0; i < 3; i++) {
        arr[i] = arr[i+1] + 5;
        printf("%d\n", i);
    }
    return 0;
}