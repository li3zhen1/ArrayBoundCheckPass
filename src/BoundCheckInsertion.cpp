#include "BoundCheckInsertion.h"
#include "CommonDef.h"

using namespace llvm;

PreservedAnalyses BoundCheckInsertion::run(Function &F,
                                           FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  // LLVMContext &Context = F.getContext();
  // Instruction *InsertPoint = F.getEntryBlock().getFirstNonPHI();
  // IRBuilder<> IRB(InsertPoint);
  // AttributeList Attr;
  // FunctionCallee Check = F.getParent()->getOrInsertFunction(
  //     "checkBound", Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
  //     IRB.getPtrTy(), IRB.getInt64Ty());

  // TODO: cache the file name
  // const auto file =
  //     IRB.CreateGlobalStringPtr(F.getParent()->getSourceFileName());
  // auto createCheckBoundCall = [&](int arraySize, Value *subscript,
  //                                 ConstantInt *line) {
  //   IRB.CreateCall(Check, {ConstantInt::get(IRB.getInt64Ty(), arraySize),
  //                          subscript, file, line});
  // };

  for (auto &BB : F) {

    for (auto &I : BB) {
      MDNode *MN = I.getMetadata(ACCESS_KEY);
      // const auto *GEP = dyn_cast<GetElementPtrInst>(&I);
      if (MN) {
        llvm::errs() << MN->getNumOperands() << "\n";
        continue;
        // "static array" or "dynamic array"
        StringRef ArrayType =
            cast<MDString>(MN->getOperand(0).get())->getString();
        // for "static array", "Bound" is a pointer to "ConstantInt"
        // for "dynamic array", "Bound" is a pointer to "Instruction" or
        // "ConstantInt" or "GlobalVariable" You can use "cast<>" to convert
        // Bound to the pointer of subclasses
        Value *Bound =
            cast<ValueAsMetadata>(MN->getOperand(1).get())->getValue();

        // IRB.SetInsertPoint(&I);

        // {
        //   llvm::errs() << "\n=========\nBound: \n";

        //   Bound->printAsOperand(llvm::errs(), false);
        //   llvm::errs() << "\n";

          // llvm::errs() << "\nI: \n";
          // I.print(llvm::errs());
          // llvm::errs() << "\n";

        //   llvm::errs() << "\nOperands: \n";
        //   for (auto &Op : I.operands()) {
        //     Op->printAsOperand(llvm::errs(), false);
        //   }
        //   llvm::errs() << "\n--------\n";
        // }

        // Value* subscript = nullptr;
        // if (GEP->getSourceElementType()->isArrayTy()) {
        //   subscript = GEP->getOperand(2);
        // } else if (GEP->getSourceElementType()->isIntegerTy()){
        //   // subscript = GEP->getOperand(1);
        // }

        if (const auto *CI = dyn_cast<ConstantInt>(Bound)) {
          // dynamic / static
          // const auto arraySize = CI->getValue();
          // createCheckBoundCall(arraySize.getZExtValue(), subscript,
          //                      IRB.getInt64(0));

        } else if (const auto *Inst = dyn_cast<Instruction>(Bound)) {
          // dynamic array
          // const auto arraySize = Inst->getOperand(0);
          // createCheckBoundCall(0, subscript, IRB.getInt64(0));

        } else if (const auto *GV = dyn_cast<GlobalVariable>(Bound)) {
          // dynamic array
          // const auto arraySize = GV->getInitializer();
          // createCheckBoundCall(0, subscript, IRB.getInt64(0));

        } else {
          // llvm_unreachable("Unknown Bound type.");
        }

        if (MN->getNumOperands() > 2) {
          // When the array type is "dynamic array", the metadata also provide a
          // reference to the associated malloc invocation
          CallBase *Allocator = cast<CallBase>(
              cast<ValueAsMetadata>(MN->getOperand(2).get())->getValue());
        }
      }
    }
  }
  return PreservedAnalyses::none();
}

BoundCheckInsertion::~BoundCheckInsertion() {}

void BoundCheckInsertion::InstrumentationExample() {}
