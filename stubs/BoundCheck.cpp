#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if the subscript is out of bound
 *
 * @param bound
 * @param subscript
 * @param file
 * @param line debug information, is not reported if line <= 0
 */
void checkBound(int bound, int subscript, const char *file, int line) {
  std::cerr << subscript << "/" << bound << std::endl;
  if (subscript < 0 || subscript >= bound) {
    std::cerr << "Array out of bound at " << file;
    if (line > 0) {
      std::cerr << "#" << line;
    }
    std::cerr << " while "
              << "subscripting " << subscript << " to array of size " << bound
              << std::endl;
  }
}

void assertLessOrEqual(int lhs, int rhs, const char *file, int line) {
  std::cerr << lhs << "≤" << rhs << std::endl;
  if (lhs > rhs) {
    std::cerr << "Assertion failed at " << file;
    if (line > 0) {
      std::cerr << "#" << line;
    }
    std::cerr << " with " << lhs << " > " << rhs << std::endl;
  }
}



#ifdef __cplusplus
}
#endif
