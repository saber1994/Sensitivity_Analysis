#include "FaultInjection.h"
using namespace llvm;
bool IndexMark::isInstructionTargetType(Instruction* instruction){
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
char IndexMark::ID = 0;
static RegisterPass<IndexMark> X(
	"IndexMark", "Mark each possible fault injection instruction's index");