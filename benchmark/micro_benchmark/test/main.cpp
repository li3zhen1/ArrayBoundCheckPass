#include <stdlib.h>



int main(int argc, char* argv[]
) {
    int i = argc;
    int sz = 199;

    int* a = (int*)malloc(sz * sizeof(int));

    a[i] = 77;

    if (argc>4) {
        a[i] = a[i] + 1;
    }

    a[i+1] = 42;
    
    return 0;
}