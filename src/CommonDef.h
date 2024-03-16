#ifndef COMMON_DEF_H
#define COMMON_DEF_H

#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/WithColor.h"
#include <cstdint>

extern const char *ACCESS_KEY;
constexpr uint64_t UNKNOWN = 0;

#define _DEBUG_PRINT 0

#define VERBOSE_PRINT if (true)


#define RedO llvm::WithColor(O).changeColor(raw_ostream::RED, true, false)
#define GreenO llvm::WithColor(O).changeColor(raw_ostream::GREEN, true, false)

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