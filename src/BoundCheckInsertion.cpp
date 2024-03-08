#include "BoundCheckInsertion.h"
#include "CommonDef.h"

using namespace llvm;

PreservedAnalyses BoundCheckInsertion::run(Function &F, FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  
  for (auto &BB : F) {
    for (auto &I : BB) {
      MDNode *MN = I.getMetadata(ACCESS_KEY);
      if (MN) {
        // "static array" or "dynamic array"
        StringRef ArrayType = cast<MDString>(MN->getOperand(0).get())->getString();
        // for "static array", "Bound" is a pointer to "ConstantInt"
        // for "dynamic array", "Bound" is a pointer to "Instruction" or "ConstantInt" or "GlobalVariable"
        // You can use "cast<>" to convert Bound to the pointer of subclasses
        Value *Bound = cast<ValueAsMetadata>(MN->getOperand(1).get())->getValue();
        if (MN->getNumOperands() > 2) {
          // When the array type is "dynamic array", the metadata also provide a reference to the
          // associated malloc invocation
          CallBase *Allocator = cast<CallBase>(cast<ValueAsMetadata>(MN->getOperand(2).get())->getValue());
        }
      }
    }
  }
  return PreservedAnalyses::none();
}

BoundCheckInsertion::~BoundCheckInsertion() {

}

void BoundCheckInsertion::InstrumentationExample() {
  
}