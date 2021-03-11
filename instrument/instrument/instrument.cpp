#include "instrument.h"
#include <fstream>
#include <sstream>
using namespace llvm;
long instrument::instruction_count = 1;
bool instrument::runOnModule(Module &M){
	std::ofstream tracefile("./InstrumentFile");
	long instruction_index;
	std::vector<long> instruction_indexes;
	tracefile<<"FunctionName"<<" ";
	tracefile<<"BasicBlockName"<<" ";
	tracefile<<"BeginIndex"<<" ";
	tracefile<<"EndIndex"<<"\n";
	int unnameIndex = 1;
	for (Module::iterator MI = M.begin(); MI != M.end(); MI++){
		if (MI->size() == 0)
            continue;
        Function &Func= *MI;
        unnameIndex = 1;
        for (BasicBlock &BB : Func){
					tracefile<<Func.getName().str()<<" ";
        	if(BB.hasName()){
        		tracefile<<BB.getName().str()<<" ";
        	}
        	else{
        		std::stringstream ssTemp;
        		ssTemp<<unnameIndex;
        		std::string tempName = "UnnameBlock"+ssTemp.str();
        		BB.setName(tempName);
        		tracefile<<BB.getName().str()<<" ";
        		unnameIndex++;
        	}
        	for(Instruction &I : BB){
        		setInstructionIndex(&I);
        		instruction_indexes.push_back(getInstructionIndex(&I));
        	}
        	if(instruction_indexes.empty())
        	{
        		tracefile<<"None"<<" ";
        		tracefile<<"None"<<"\n";
        	}
        	else{
        		tracefile<<instruction_indexes.front()<<" ";
        		tracefile<<instruction_indexes.back()<<"\n";
        	}
        	instruction_indexes.clear();
        }
	}
}

bool instrument::isInstructionTargetType(Instruction* instruction){
    //Data instruction
    if (instruction->isBinaryOp()||(instruction->getOpcode()>=20&&instruction->getOpcode()<=25)
        ||instruction->getOpcode()==27||instruction->getOpcode()==29)
        return true;
    //Branch instruction
    if (instruction->getOpcode()==2)
        return true;
    //Memory instruction
    if (instruction->getOpcode()==28)
        return true;
    return false;
}

void instrument::setInstructionIndex(Instruction *inst) {
  	Function *func = inst->getParent()->getParent();
  	LLVMContext &context = func->getContext();
  	MDNode *mdnode = MDNode::get(context, ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(context), instruction_count++)));
  	inst->setMetadata("instruction_index", mdnode);
}

long instrument::getInstructionIndex(Instruction *inst) {
  MDNode *mdnode = inst->getMetadata("instruction_index");
  if (mdnode) {
    ConstantInt *cns_index = dyn_cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(mdnode->getOperand(0))->getValue());
    return cns_index->getSExtValue();
  } else {
    errs() << "ERROR: Instruction indices for instructions are required for the pass\n";
    exit(3);
  }
}

char instrument::ID = 0;
static RegisterPass<instrument> X(
    "instrument", "Instrument the source program");
