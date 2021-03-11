#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Constants.h"

using namespace llvm;
namespace fault{
	void setInstructionIndex(Instruction *inst);
	long getInstructionIndex(Instruction *inst);
}