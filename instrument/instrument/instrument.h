#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Pass.h"
#include "llvm/IR/BasicBlock.h"

using namespace llvm;
class instrument : public  ModulePass{
    public:
	    static char ID;
	    static long instruction_count;
	    instrument() : ModulePass(ID) {}
	    bool runOnModule(Module &M);
	private:
		void setInstructionIndex(Instruction *inst);
		long getInstructionIndex(Instruction *inst);
		bool isInstructionTargetType(Instruction* inst);
};