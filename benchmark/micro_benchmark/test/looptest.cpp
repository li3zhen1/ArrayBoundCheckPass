#include <stdlib.h>
int main(int argc, char *argv[]) {
  int size = atoi(argv[1]);
  auto arr = (int *)malloc(size * sizeof(int));
  
  for (int i = 0; i < size + 1; i++) {
    int k = i*0;
    arr[k] = 77;
  }
  return 0;
}