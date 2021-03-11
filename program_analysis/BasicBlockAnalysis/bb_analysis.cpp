#include"bb_analysis.h"
#include "llvm/IR/CFG.h"
#include <algorithm>
#include <fstream>
using namespace llvm;
namespace {
void BasicBlockAnalysis::traceFileGenerate(std::vector<BBInformation*> bb_informations){
    std::ofstream tracefile("./basicBlockTrace");
    if(tracefile.fail())
        errs()<<"File Open Error!\n";
    else{
        tracefile<<"Function|Name|Pre|Post|Input|Output|ExternalCall|Initial|Return|Memory|Total|IDistance|RDistance"<<std::endl;
        for(BBInformation* information : bb_informations){
            tracefile<<information->get_function_name()<<" ";
            tracefile<<information->get_bb_name()<<" ";
            tracefile<<information->get_pre_bb_number()<<" ";
            tracefile<<information->get_post_bb_number()<<" ";
            tracefile<<information->get_input_number()<<" ";
            tracefile<<information->get_output_number()<<" ";
            tracefile<<information->get_has_external_call()<<" ";
            tracefile<<information->get_is_initial_block()<<" ";
            tracefile<<information->get_is_ret_block()<<" ";
            tracefile<<information->get_memory_inst_number()<<" ";
            tracefile<<information->get_total_instruction_number()<<" ";
            tracefile<<information->get_inital_distance()<<" ";
            tracefile<<information->get_ret_distance()<<std::endl;
        }
        errs()<<"Trace file has been written.\n";
        tracefile.close();
    }
}



std::map<std::string,dataFlow>  BasicBlockAnalysis::dataFlowAnalysis(Function &Func){
    std::map<std::string, dataFlow> dataFlowInfo;
    for (BasicBlock &BB : Func){
        dataFlow bb_flow;
        bb_flow.gen = generation(BB);
        bb_flow.kill = kill(BB);
        //initate the exit basic block
        if(BB.getTerminator()->getOpcode()==1){
            INSTRUCTIONSET temp;
            std::set_union(bb_flow.bb_input.begin(),bb_flow.bb_input.end(),
                            bb_flow.gen.begin(),bb_flow.gen.end(),
                            std::inserter(temp,temp.begin()),compareTwoInstructions);
            bb_flow.bb_input = temp;
        }
        dataFlowInfo.insert(std::pair<std::string,dataFlow>(BB.getName().str(),bb_flow));
    }
    bool keep_iteration = true;
    Function::iterator e;
    Function::iterator be;
    //when this iteration do not change any inputs or outputs of the block, stop iteration
    while(keep_iteration){
        keep_iteration = false;
        //backward anlaysis need to iterate the basic block list from the end
        e =  Func.end();
        be = Func.begin();
        do{
            e--;
            BasicBlock* BB = &*e;
            std::map<std::string, dataFlow>::iterator it =
              dataFlowInfo.find(BB->getName().str());
            if(it == dataFlowInfo.end()){
                errs()<<"Can not find data flow information!\n";
                break;
            }
            //dataFlow cur_flow = it->second;
            INSTRUCTIONSET temp;
            //Basic Block output traced (the exit block do not need to be processed again)
            if(BB->getTerminator()->getOpcode()!=1){
                for(BasicBlock*suc : successors(BB)){
                    dataFlow temp_flow = dataFlowInfo.find(suc->getName().str())->second;
                    std::set_union(temp.begin(),temp.end(),temp_flow.bb_input.begin(),
                    temp_flow.bb_input.end(),std::inserter(temp,temp.begin()),compareTwoInstructions);
                }

                if(temp != dataFlowInfo[BB->getName().str()].bb_output){
                    keep_iteration = true;
                    dataFlowInfo[BB->getName().str()].bb_output = temp;

                    //Basic Block input traced
                    dataFlowInfo[BB->getName().str()].bb_input = dataFlowInfo[BB->getName().str()].bb_output;
                    temp.clear();
                    for(Instruction* value : dataFlowInfo[BB->getName().str()].kill){
                        INSTRUCTIONSET::iterator ite = dataFlowInfo[BB->getName().str()].bb_input.find(value);
                        if(ite!=dataFlowInfo[BB->getName().str()].bb_input.end())
                            dataFlowInfo[BB->getName().str()].bb_input.erase(ite);
                    }
                    std::set_union(dataFlowInfo[BB->getName().str()].bb_input.begin(),dataFlowInfo[BB->getName().str()].bb_input.end(),
                        dataFlowInfo[BB->getName().str()].gen.begin(),dataFlowInfo[BB->getName().str()].gen.end(),
                        std::inserter(temp,temp.begin()),compareTwoInstructions);
                    dataFlowInfo[BB->getName().str()].bb_input = temp;
                }
            }
        }while(e!=be);
    }
    errs()<<"Function: "<<Func.getName().str()<<" analysis is done\n";
    return dataFlowInfo;
}

INSTRUCTIONSET BasicBlockAnalysis::generation(BasicBlock& BB){
    INSTRUCTIONSET v;
    for(Instruction &I : BB){
        if(I.getOpcode() == 2 && I.getNumOperands() == 3){
            if(auto* inst = dyn_cast<Instruction>(I.getOperand(0)))
                v.insert(inst);
        }
        else if(I.isBinaryOp()){
            for(int i=0;i<I.getNumOperands();i++){
                if(auto* inst = dyn_cast<Instruction>(I.getOperand(i)))
                    v.insert(inst);
            }
        }
        else if(I.getOpcode() == 54){
            for(int i=0;i<I.getNumOperands();i++){
                if(auto* inst = dyn_cast<Instruction>(I.getOperand(i)))
                    v.insert(inst);
            }
        }
    }
    return v;
}

INSTRUCTIONSET BasicBlockAnalysis::kill(BasicBlock& BB){
    INSTRUCTIONSET v;
    for(Instruction &I : BB){
        if(I.isBinaryOp()){
            v.insert(&I);
        }
        else if(I.getOpcode()==30){
            v.insert(&I);
        }
        else if(I.getOpcode()==54){
            v.insert(&I);
        }
    }
return v;
}

BBInformation* BasicBlockAnalysis::getBasicBlockInformation(BasicBlock &BB){
    bool has_external_call=false;
    int pre_bb_number=0;
    int post_bb_number=0;
    int shortest_depth=0;
    int memory_inst_number=0;
    int total_instruction_number=0;

    BBInformation *info = new BBInformation();
    info->set_bb_name(BB.getName().str());
    info->set_function_name(BB.getParent()->getName().str());

    for (BasicBlock *Pred : predecessors(&BB)) {
        pre_bb_number++;
    }
    for (BasicBlock *suc : successors(&BB)) {
        post_bb_number++;
    }
    info->set_pre_bb_number(pre_bb_number);
    info->set_post_bb_number(post_bb_number);

    for(Instruction &I :BB){
        if(I.getOpcode()>=29&&I.getOpcode()<=35)
            memory_inst_number++;
        else if(I.getOpcode()==54)
            has_external_call = true;
            total_instruction_number++;
    }
    info->set_memory_inst_number(memory_inst_number);
    info->set_total_instruction_number(total_instruction_number);
    info->set_has_external_call(has_external_call);
    return info;
}

#define INF 99999

std::map<std::string,std::map<std::string,shortDistance>> BasicBlockAnalysis::floydWarshall (Function &Func)
{
  std::map<std::string, std::map<std::string ,size_t>> block_distances;
  //short distance of one basic block
  std::map<std::string, size_t> distance;
  
  for(BasicBlock &BB : Func){
      distance.insert(std::pair<std::string,size_t>(BB.getName().str(),INF));
  }

  for(BasicBlock &BB : Func)
  {
      std::map<std::string ,size_t> temp = distance;
      for (BasicBlock *suc : successors(&BB)) {
          temp[suc->getName().str()] = suc->size();
      }
      block_distances.insert(std::pair<std::string,std::map<std::string, size_t>>(BB.getName().str(),temp));
  }

    std::map<std::string, std::map<std::string ,shortDistance>> shortDistances;
    //record the shortest path
    std::map<std::string, std::map<std::string,std::string >> f;
    for(BasicBlock &BBi : Func)
    {
        std::map<std::string, std::string> temp;
        for(BasicBlock &BBj : Func){
            temp.insert(std::pair<std::string,std::string>(BBj.getName().str(),BBj.getName().str()));
        }
        f.insert(std::pair<std::string, std::map<std::string ,std::string>>(BBi.getName().str(),temp));

    }
    for (BasicBlock &BBk : Func)
    {
        // Pick all vertices as source one by one
        for (BasicBlock &BBi : Func)
        {
            // Pick all vertices as destination for the
            // above picked source
            for (BasicBlock &BBj : Func)
            {
                // If vertex k is on the shortest path from
                // i to j, then update the value of dist[i][j]
                if (block_distances[BBi.getName().str()][BBk.getName().str()]+block_distances[BBk.getName().str()][BBj.getName().str()]<block_distances[BBi.getName().str()][BBj.getName().str()]){
                    block_distances[BBi.getName().str()][BBj.getName().str()] = block_distances[BBi.getName().str()][BBk.getName().str()]+block_distances[BBk.getName().str()][BBj.getName().str()];
                    f[BBi.getName().str()][BBj.getName().str()] = BBk.getName().str();
                }
            }
        }
    }
    for(BasicBlock &BBi : Func){
        std::map<std::string,shortDistance> temp;
        for(BasicBlock &BBj : Func){
           std::vector<std::string> path;
           shortDistance info;
           info.distance = block_distances[BBi.getName().str()][BBj.getName().str()];
           path.push_back(BBi.getName().str());
           std::string des= BBj.getName().str();
           while(f[BBi.getName().str()][des] != des){
                path.push_back(f[BBi.getName().str()][des]);
                des = f[BBi.getName().str()][des];
           }
           path.push_back(BBj.getName().str());
           info.path = path;
           temp.insert(std::pair<std::string,shortDistance>(BBj.getName().str() ,info));
        }
         shortDistances.insert(
            std::pair<std::string,std::map<std::string,shortDistance>>(BBi.getName().str(),temp));
    }
    return shortDistances;
}
}
char BasicBlockAnalysis::ID = 0;
static RegisterPass<BasicBlockAnalysis> X(
    "BasicBlockAnalysis", "Get the basic block information of the target program");
