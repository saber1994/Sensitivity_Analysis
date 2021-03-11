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
#include<string>
#include <map>
#include <set>
#include <vector>

using namespace llvm;
namespace {
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

    struct dataFlow
    {
        INSTRUCTIONSET bb_input;
        INSTRUCTIONSET bb_output;
        INSTRUCTIONSET gen;
        INSTRUCTIONSET kill;
    };

    struct shortDistance
    {
        size_t distance;
        std::vector<std::string> path;
    };

    class BBInformation{
        private:
            int pre_bb_number;
            int post_bb_number;
            int memory_inst_number;
            int input_number;
            int output_number;
            int shortest_depth;
            int total_instruction_number;
            int inital_distance;
            int ret_distance;
            bool has_external_call;
            bool is_initial_block;
            bool is_ret_block;
            bool is_cond_branch;
            std::string bb_name;
            std::string function_name;

        public:
            BBInformation(){}
            void set_function_name(std::string name){this->function_name=name;}
            std::string get_function_name(){return this->function_name;}
            void set_bb_name(std::string name){this->bb_name = name;}
            std::string get_bb_name(){return this->bb_name;}
            void set_pre_bb_number(int number){this->pre_bb_number = number;}
            int get_pre_bb_number(){return this->pre_bb_number;}
            void set_post_bb_number(int number){this->post_bb_number = number;}
            int get_post_bb_number(){return this->post_bb_number;}
            void set_memory_inst_number(int number){this->memory_inst_number = number;}
            int get_memory_inst_number(){return this->memory_inst_number;}
            void set_input_number(int number){this->input_number = number;}
            int get_input_number(){return this->input_number;}
            void set_output_number(int number){this->output_number = number;}
            int get_output_number(){return this->output_number;}
            void set_shortest_depth(int depth){this->shortest_depth = depth;}
            int get_shortest_depth(){return this->shortest_depth;}
            void set_total_instruction_number(int number){this->total_instruction_number = number;}
            int get_total_instruction_number(){return this->total_instruction_number;}
            void set_is_cond_branch(bool flag){this->is_cond_branch = flag;}
            int get_set_is_cond_branch(){return this->is_cond_branch;}
            void set_has_external_call(bool flag){this->has_external_call = flag;}
            bool get_has_external_call(){return this->has_external_call;}
            void set_is_initial_block(bool flag){this->is_initial_block = flag;}
            bool get_is_initial_block(){return this->is_initial_block;}
            void set_is_ret_block(bool flag){this->is_ret_block = flag;}
            bool get_is_ret_block(){return this->is_ret_block;}
            void set_inital_distance(int distance){this->inital_distance = distance;}
            int get_inital_distance(){return this->inital_distance;}
            void set_ret_distance(int distance){this->ret_distance = distance;}
            int get_ret_distance(){return this->ret_distance;}
    };

    class BasicBlockAnalysis : public  ModulePass{
        public:
            static char ID;
            BasicBlockAnalysis () : ModulePass(ID) {}
        private:
            bool runOnModule(Module &M) override{
                int blockcount = 0;
                std::vector<BBInformation*> bb_informations;
                for (Module::iterator MI = M.begin(); MI != M.end(); MI++){
                    blockcount = 0;
                    if (MI->size() == 0)
                        continue;
                    Function &Func= *MI;
                    std::map<std::string, dataFlow> dataFlowInfo = dataFlowAnalysis(Func);

                    std::string inital_block_name;
                    std::string ret_block_name;
                    for(BasicBlock &BB : Func){
                      if (blockcount == 0){
                          inital_block_name = BB.getName().str();
                      }
                      Instruction* terminator = BB.getTerminator();
                      if (terminator->getOpcode() == 1){
                          ret_block_name = BB.getName().str();
                      }
                      blockcount++;
                    }
                    std::map<std::string,std::map<std::string,shortDistance>> shortDistances = floydWarshall(Func);
                    blockcount = 0;
                    for (BasicBlock &BB : Func){
                        bool is_initial_block = false;
                        bool is_ret_block = false;
                        if (blockcount == 0){
                            is_initial_block = true;
                        }
                        Instruction* terminator = BB.getTerminator();
                        if (terminator->getOpcode() == 1){
                            is_ret_block =true;
                        }

                        BBInformation *information = getBasicBlockInformation(BB);
                        information->set_is_initial_block(is_initial_block);
                        information->set_is_ret_block(is_ret_block);
                        if(is_initial_block){
                          information->set_inital_distance(0);
                        }
                        else{
                          information->set_inital_distance(shortDistances[inital_block_name][BB.getName().str()].distance);
                        }
                        if(is_ret_block){
                          information->set_ret_distance(0);
                        }
                        else{
                          information->set_ret_distance(shortDistances[BB.getName().str()][ret_block_name].distance);
                        }

                        std::map<std::string, dataFlow>::iterator it = dataFlowInfo.find(BB.getName().str());
                        if(it == dataFlowInfo.end())
                            errs()<<"Can not find data flow information!";
                        dataFlow bb_flow = dataFlowInfo.find(BB.getName().str())->second;
                        information->set_input_number(bb_flow.bb_input.size());
                        information->set_output_number(bb_flow.bb_output.size());
                        blockcount++;
                        bb_informations.push_back(information);
                    }
                }
                traceFileGenerate(bb_informations);
                return true;
            }
            void traceFileGenerate(std::vector<BBInformation*> bb_informations);
            std::map<std::string, dataFlow> dataFlowAnalysis(Function &Func);
            INSTRUCTIONSET generation(BasicBlock& BB);
            INSTRUCTIONSET kill(BasicBlock& BB);
            BBInformation* getBasicBlockInformation(BasicBlock &BB);
            std::map<std::string,std::map<std::string,shortDistance>> floydWarshall (Function &Func);
    };
}
