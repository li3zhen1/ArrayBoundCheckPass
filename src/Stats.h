#pragma once


#include "llvm/IR/Module.h"
#include "CommonDef.h"

using namespace llvm;


struct CheckCount {
  int lbCount;
  int ubCount;
};

CheckCount CountBountCheck(Function& F, const char* tableName);

void DumpCheckCount(Function& F, const char *dst, const char* entryName, const CheckCount& CheckStat);