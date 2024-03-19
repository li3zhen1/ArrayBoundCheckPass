static int a[200] = {0};
static int b[20][12] = {0};

void tt(int t[20][20]) { t[1][1] = 1; }

int main() {
  int local1d[200] = {0};
  local1d[42] = 77;
  a[1] = 1;
  b[1][1] = 1;
}