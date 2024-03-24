#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/WithColor.h"
#include "llvm/Support/raw_ostream.h"
#include <cstdint>

extern const char *ACCESS_KEY;
constexpr uint64_t UNKNOWN = 0;

constexpr auto SOURCE_FILE_NAME = "__source_file_name__";

constexpr auto CHECK_LB = "checkLowerBound";
constexpr auto CHECK_UB = "checkUpperBound";

#define _DEBUG_PRINT 0

// #ifdef VERBOSE_PRINT_LEVEL
// #else
#define VERBOSE_PRINT_LEVEL 0
// #endif

#define VERBOSE_PRINT if (VERBOSE_PRINT_LEVEL)

// #ifdef MODIFICATION
// #else
#define MODIFICATION true
// #endif

// #ifdef ELIMINATION
// #else
#define ELIMINATION true
// #endif

// #ifdef LOOP_PROPAGATION
// #else
#define LOOP_PROPAGATION true
// #endif

// #ifdef CLEAN_REDUNDANT_CHECK_IN_SAME_BB
// #else
#define CLEAN_REDUNDANT_CHECK_IN_SAME_BB ELIMINATION
// #endif

// #ifndef DUMP_STATS
#define DUMP_STATS true
// #endif

#define RedO llvm::WithColor(O).changeColor(raw_ostream::RED, true, false)
#define GreenO llvm::WithColor(O).changeColor(raw_ostream::GREEN, true, false)

#define BLUE(O) llvm::WithColor(O).changeColor(raw_ostream::BLUE, true, false)
#define MAGENTA(O)                                                             \
  llvm::WithColor(O).changeColor(raw_ostream::MAGENTA, true, false)
#define YELLOW(O)                                                              \
  llvm::WithColor(O).changeColor(raw_ostream::YELLOW, true, false)

// #undef _DEBUG_PRINT

enum ArrayCategory : unsigned {
  SingleDimensionalArray = 0,
  MultiDimensionalArray = 1,
  DynamicArray = 2
};

// const char *ArrayCategoryDesc[] = {
//     "single-dimensional array",
//     "multi-dimensional array",
//     "dynamically allocated array"
// };

bool isCxxSTLFunc(llvm::StringRef Name);

bool isCProgram(llvm::Module *M);

llvm::raw_ostream &verboseOut();

#endif // COMMON_DEF_H