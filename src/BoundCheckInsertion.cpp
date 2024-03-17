#include "BoundCheckInsertion.h"
#include "CommonDef.h"

using namespace llvm;



PreservedAnalyses BoundCheckInsertion::run(Function &F,
                                           FunctionAnalysisManager &FAM) {
  if (!isCProgram(F.getParent()) && isCxxSTLFunc(F.getName())) {
    return PreservedAnalyses::all();
  }
  LLVMContext &Context = F.getContext();
  Instruction *InsertPoint = F.getEntryBlock().getFirstNonPHI();
  IRBuilder<> IRB(InsertPoint);
  AttributeList Attr;
  
  FunctionCallee Check = F.getParent()->getOrInsertFunction(
      "checkBound", Attr, IRB.getVoidTy(), IRB.getInt64Ty(), IRB.getInt64Ty(),
      IRB.getPtrTy(), IRB.getInt64Ty());

  // TODO: cache the file name
  const auto file =
      IRB.CreateGlobalStringPtr(F.getParent()->getSourceFileName());


  auto createCheckBoundCall = [&](Instruction *point, Value* arraySize,
                                  Value *subscript) {
    Value* ln;
    if (const auto Loc = point->getDebugLoc()) {
      ln = IRB.getInt64(Loc.getLine());
    } else {
      ln = IRB.getInt64(0);
    }
    IRB.SetInsertPoint(point);
    IRB.CreateCall(Check, {arraySize, subscript, file, ln});
  };

  for (auto &BB : F) {

    for (auto &I : BB) {
      MDNode *MN = I.getMetadata(ACCESS_KEY);
      const auto *GEP = dyn_cast<GetElementPtrInst>(&I);
      if (MN) {
        // "static array" or "dynamic array"
        StringRef ArrayType =
            cast<MDString>(MN->getOperand(0).get())->getString();
        // for "static array", "Bound" is a pointer to "ConstantInt"
        // for "dynamic array", "Bound" is a pointer to "Instruction" or
        // "ConstantInt" or "GlobalVariable" You can use "cast<>" to convert
        // Bound to the pointer of subclasses
        Value *Bound =
            cast<ValueAsMetadata>(MN->getOperand(1).get())->getValue();

        Value *subscript = nullptr;
        if (GEP->getSourceElementType()->isArrayTy()) {
          subscript = GEP->getOperand(2);
        } else if (GEP->getSourceElementType()->isIntegerTy()) {
          subscript = GEP->getOperand(1);
        } else {
          
          // GEP->print(errs());
          subscript = GEP->getOperand(1);
          // throw std::runtime_error("Unknown GEP type.");
        }
        
        createCheckBoundCall(&I, Bound, subscript);


        // FIXME: Not used
        // Value* mallocSizeIfExist = nullptr;
        // if (MN->getNumOperands() > 2) {
        //   // When the array type is "dynamic array", the metadata also provide a
        //   // reference to the associated malloc invocation
        //   CallBase *Allocator = cast<CallBase>(
        //       cast<ValueAsMetadata>(MN->getOperand(2).get())->getValue());
        //   mallocSizeIfExist = Allocator->getOperand(0);
        //   // this should be size_of_type * array_size
        // }

        // if (const auto *CI = dyn_cast<ConstantInt>(Bound)) {
        //   // dynamic / static
        //   // const auto arraySize = CI->getValue();
        //   // const auto arraySizeValue = ConstantInt::get(IRB.getInt64Ty(), arraySize.getZExtValue());
        //   createCheckBoundCall(&I, Bound, subscript);

        // } else if (const auto *Inst = dyn_cast<Instruction>(Bound)) {
        //   // dynamic array
        //   // const auto arraySize = Inst->getOperand(0);
        //   createCheckBoundCall(&I, Bound, subscript);

        // } else if (const auto *GV = dyn_cast<GlobalVariable>(Bound)) {
        //   // dynamic array
        //   // const auto arraySize = GV->getInitializer();
        //   createCheckBoundCall(&I, Bound, subscript);
        // } else {
        //   llvm_unreachable("Unknown Bound type.");
        // }
      }
    }
  }
  return PreservedAnalyses::none();
}

BoundCheckInsertion::~BoundCheckInsertion() {}

void BoundCheckInsertion::InstrumentationExample() {}
