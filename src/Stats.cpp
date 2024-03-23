#include "Stats.h"
#include "CommonDef.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/WithColor.h"
#include <fstream>
#include <string>

// static DenseMap<std::string, CheckCount> CheckStats;

CheckCount CountBountCheck(Function &F, const char *tableName) {
  int lbCount = 0;
  int ubCount = 0;
  for (auto &BB : F) {
    for (auto &I : BB) {
      if (isa<CallInst>(&I)) {
        auto *CI = cast<CallInst>(&I);
        auto FName = CI->getCalledFunction()->getName();
        if (FName == CHECK_LB) {
          lbCount++;
        } else if (FName == CHECK_UB) {
          ubCount++;
        }
      }
    }
  }

  MAGENTA(llvm::errs())
      << "╭─────────────────────────────────────────────────╮\n";
  MAGENTA(llvm::errs()) << "│ " << tableName << "\n";
  MAGENTA(llvm::errs()) << "│ Lower Bound Check: " << lbCount << "\n";
  MAGENTA(llvm::errs()) << "│ Upper Bound Check: " << ubCount << "\n";
  MAGENTA(llvm::errs()) << "│ Total Bound Check: " << lbCount + ubCount << "\n";
  MAGENTA(llvm::errs())
      << "╰─────────────────────────────────────────────────╯\n";

  DumpCheckCount(F, getenv("DUMP_DST"), tableName, {lbCount, ubCount});

  return {lbCount, ubCount};
}

void DumpCheckCount(Function &F, const char *dst, const char *entryName,
                    const CheckCount &CheckStat) {
  std::ofstream file;
  file.open(dst, std::ios_base::app);

  llvm::errs() << "Dumping stats to " << dst << "\n";

  file << F.getName().str() << ", " << entryName << ", " << CheckStat.lbCount
       << ", " << CheckStat.ubCount << ", "
       << CheckStat.lbCount + CheckStat.ubCount << "\n";
}
