#include <stdio.h>
int main() {
    
    int arr[4] = {1, 2, 3, 4};

    int v = 1;
    int t = 2;

    for (int i = 0; i < 3; i++) {

        arr[i] = arr[v * i + t] + 5;
        printf("%d\n", i);
    }
    return 0;
}