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
void checkBound(int bound, int subscript, const char *file,
                           int line) {
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

#ifdef __cplusplus
}
#endif
