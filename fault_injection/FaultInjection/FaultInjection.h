#include <vector>
#include <set>
#include <map>
#include <string.h>
#include <sstream>
#include "llvm/IR/BasicBlock.h"
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



using namespace llvm;
namespace llvm{

    long getInstructionIndex(const Instruction *inst) {
        MDNode *mdnode = inst->getMetadata("instruction_index");
        if (mdnode) {
            ConstantInt *cns_index = dyn_cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(mdnode->getOperand(0))->getValue());
            return cns_index->getSExtValue();
        } else {
            errs() << "ERROR: Instruction indices for instructions are required for the pass\n";
            exit(3);
        }
    }

    inline bool compareTwoInstructions(Instruction* v1, Instruction* v2){
        return getInstructionIndex(v1)==getInstructionIndex(v2);
    }

    struct instruction_compare
    {
        bool operator() (const Instruction* lhs, const Instruction* rhs) const{
            return getInstructionIndex(lhs)<getInstructionIndex(rhs);
        }
    };


    typedef std::set<Instruction*, instruction_compare> INSTRUCTIONSET;

    struct shortDistance
    {
        size_t distance;
        std::vector<std::string> path;
    };

    struct faultPoint
    {
        Instruction* instruction;
        int numofBits;
    };

	static cl::opt<int> instructionIndex("instructionIndex", cl::desc("The index of target instruction"),
    cl::value_desc("Target instruction index"), cl::init(0), cl::ValueOptional);
    static cl::opt<std::string> configurationFile("configurationFile", cl::desc("The location of the fault injection file."),
    cl::value_desc("FaultInjection configuration file"), cl::init(" "), cl::ValueOptional);

    class FaultInjection : public  ModulePass{
        public:
            static char ID;
            static long fault_count;
            FaultInjection() : ModulePass(ID) {}
            bool runOnModule(Module &M);
        private:
            void setFaultIndex(Instruction *inst);
            long getFaultIndex(const Instruction *inst);
            bool isInstructionTargetType(Instruction* instruction);
            bool faultInjection(Instruction &target, Module &M);
            void doMultiFaultInjection(Instruction* beginInstruction,int numofBits,int windowSize,Module &M);
            void doDataErrorInject(Instruction &target,Instruction &cond,Module &M);
            void doMemoryErrorInject(Instruction &target,Instruction &cond,Module &M);
            BasicBlock* constructPrintfBlock(BasicBlock* post_block,Module &M);
            //This variable represents the instruction index and register location
            std::map<Instruction*, std::vector<int>> fault_location;
            std::map<std::string, std::string> function_name_for_type;
            Instruction* insertFaultInjectionFunctionCall (Instruction* target, Instruction* cond, Module &M);
            void createFaultInjectionFunction(Type* target_type,Type* cond_type ,Module &M);

            Instruction* insertFaultCallforMulti (Instruction* target,int numofBits, bool singleInstruction, Module &M);
            void createFaultInjectionFunctionforMulti (Type* target_type,GlobalVariable* curFault, Module &M);
            std::vector<faultPoint> getTargetInstructions(Instruction* beginInstruction,int numofBits,int windowSize);
    };
}
