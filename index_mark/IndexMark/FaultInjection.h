#include <vector>
#include <string.h>
#include <sstream>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstrTypes.h"

using namespace llvm;
namespace {
	class IndexMark : public  ModulePass{
		public:
            static char ID;
            static bool isInstructionTargetType(Instruction *instruction);
	        IndexMark () : ModulePass(ID) {}
	    private:
			bool runOnModule(Module &M) override{
				int block_index = 1;
				int instruction_index = 1;
				std::string block_index_str = "";
				std::string instruction_index_str = "";
				std::stringstream  ss;
				std::string temp_str;
                for (Module::iterator MI = M.begin(); MI != M.end(); MI++){
                    if (MI->size() == 0)
                        continue;
                    Function &Func= *MI;
                    for (BasicBlock &BB : Func){
                        for (Instruction &I : BB){
                            if(isInstructionTargetType(&I)){
                                ss<<block_index;
                                ss>>temp_str;
                                instruction_index_str = "Instruction"+temp_str;
                                I.setMetadata(instruction_index_str,NULL);
                            	instruction_index++;
                            }
                        }
                        ss<<block_index;
                        ss>>temp_str;
                        block_index_str = "Block"+ temp_str;
                        BB.setName(block_index_str);
                        block_index++;   
                    }
			    }
			    return true;
		    }

	};
}