#include <stdio.h>

extern "C" void checkBound(int bound, int subscript, const char *file, int line);

int main() {
    int a[10];
    int i;
    for (i = 0; i <= 10; i++) {
        checkBound(10, i, __FILE__, __LINE__);
        a[i] = i;
    }
    return 0;
}