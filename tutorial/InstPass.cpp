#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/ADT/StringRef.h"

#define REGISTER_FUNC_PASS(PASS_BUILDER, NAME, CLASS)                  \
  do {                                                                 \
    PASS_BUILDER.registerPipelineParsingCallback(                      \
      [](StringRef Name, FunctionPassManager &FPM,                     \
         ArrayRef<PassBuilder::PipelineElement>) {                     \
        if (Name == #NAME) {                                           \
          FPM.addPass(CLASS());                                        \
          return true;                                                 \
        }                                                              \
        return false;                                                  \
      });                                                              \
  } while (0)
  
using namespace llvm;

class InstrumentInput : public PassInfoMixin<InstrumentInput>
{
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    if (F.getName() == "main") {
      // insert the check at the insertion point (before the instruction)
      Instruction *InsertPoint = F.getEntryBlock().getFirstNonPHI();
      IRBuilder<> IRB(InsertPoint);
      // insert the declaration of the user-defined check function
      // function declaration: void checkInput(int argc, char *argv[])
      AttributeList Attr;
      FunctionCallee Check = F.getParent()->getOrInsertFunction("checkInput", Attr, IRB.getVoidTy(), IRB.getInt32Ty(), IRB.getPtrTy());
      
      // You can also first define a FunctionType and use it to insert/retrieve the FunctionCallee pointer (same as what you have done in part 3.4)
      // FunctionType *CheckType = FunctionType::get(IRB.getVoidTy(), {IRB.getInt32Ty(), IRB.getPtrTy()}, false);
      // FunctionCallee Check = F.getParent()->getOrInsertFunction("checkInput", CheckType);

      // main() has 0 or 2 input parameters
      if (F.arg_size()) {
        IRB.CreateCall(Check, {F.getArg(0), F.getArg(1)});
      } else {
        // all variables passed to "CreateCall" as checkInput's arguments shoudle has the type of "llvm::Value *"
        // The following subclasses of llvm::Value are commonly used when inserting function calls into a module
        // llvm::Constant*       : a literal
        // llvm::Instruction*    : the return value of a llvm instruction
        // llvm::GlobalVariable* : global variables in the module

        IRB.CreateCall(Check, {ConstantInt::getSigned(IRB.getInt32Ty(), 0), ConstantPointerNull::get(IRB.getPtrTy())});
      }
    }
    return PreservedAnalyses::none();
  }
  static bool isRequired() { return true; }
};

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "CS6241_PROJ1_TUTORIAL", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            REGISTER_FUNC_PASS(PB, input-check, InstrumentInput);
          }};
}
