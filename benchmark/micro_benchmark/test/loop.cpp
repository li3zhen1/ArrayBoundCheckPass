
#define N 1000

int main(int argc, char *argv[]) {
  int array[N];
  
  for (int i = 0; i < N + 1; i++) {
    array[i + 1] = i;
  }

  return 0;
}