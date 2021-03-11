#include "PreProcess.h"
#include "llvm/Support/raw_ostream.h"

static long fi_index = 1;
void setInstructionIndex(Instruction *inst) {
  	assert (fi_index >= 0 && "static instruction number exceeds index max");
  	Function *func = inst->getParent()->getParent();
  	LLVMContext &context = func->getContext();
  	MDNode *mdnode = MDNode::get(context, ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(context), fi_index++)));
  	inst->setMetadata("instruction_index", mdnode);
}

long getInstructionIndex(Instruction *inst) {
  MDNode *mdnode = inst->getMetadata("instruction_index");
  if (mdnode) {
    ConstantInt *cns_index = dyn_cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(mdnode->getOperand(0))->getValue());
    return cns_index->getSExtValue();
  } else {
    errs() << "ERROR: Instruction indices for instructions are required for the pass\n";
    exit(3);
  }
}