#include "FaultInjection.h"
#include "PreProcess.h"
#include "llvm/IR/CFG.h"
#include <fstream>
#include <sys/time.h>

using namespace llvm;
namespace llvm{
bool FaultInjection::runOnModule(Module &M){
	int insrtuction_index = 0;
    std::ifstream inputFile;
    inputFile.open(configurationFile);
    char* strs;
    const char* delim = " ";
    if(inputFile.is_open()){
        errs()<<"ConfigurationFile has beed loaded, the other option has been ignored!\n";
        std::string line;
        int beginIndex = -1;
        int windowSize = -1;
        int numofBits = -1;
        while(std::getline(inputFile,line)){
            strs = new char[line.length()+1];
            strcpy(strs, line.c_str());
            char* p = strtok(strs,delim);
            std::string cloName = p;
            p = strtok(NULL,delim);
            std::string value = p;
            if (cloName=="beginIndex")
                beginIndex = atoi(value.c_str());
            else if (cloName=="windowSize")
                windowSize = atoi(value.c_str());
            else if (cloName=="numofBits")
                numofBits = atoi(value.c_str());
            else
            {
                errs()<<"Can not recongnize the column name\n";
                return false;
            }
        }
        if(beginIndex==-1||windowSize==-1||numofBits==-1){
            errs()<<"The configurationFile has format error\n";
            return false;
        }
        for (Module::iterator MI = M.begin(); MI != M.end(); MI++){
            if (MI->size() == 0)
                continue;
            Function &Func= *MI;
            for (BasicBlock &BB : Func){
                for(Instruction &I : BB){
                    insrtuction_index = getInstructionIndex(&I);
                    if(insrtuction_index==beginIndex){
												if(!isInstructionTargetType(&I)){
													errs()<<"The selected instruction is not the supported type\n";
													return false;
												}
                        doMultiFaultInjection(&I,numofBits,windowSize,M);
												errs()<<"Fault injection has succeeded!\n";
                        return true;
                    }
                }
            }
        }
        inputFile.close();
    }
    else{
        for (Module::iterator MI = M.begin(); MI != M.end(); MI++){
            if (MI->size() == 0)
                continue;
            Function &Func= *MI;
            for (BasicBlock &BB : Func){
        	    for(Instruction &I : BB){
                    insrtuction_index = getInstructionIndex(&I);
                    if(insrtuction_index==instructionIndex){
            	        if (faultInjection(I,M))
                            errs()<<"Fault hase been Injected!"<<"\n";
            	        else
            	            errs()<<"Fault injection Failed"<<"\n";
            	        return true;
                    }
                }
            }
        }
    }
    return false;
}

long FaultInjection::fault_count = 1;
//get the insrtuction index in the metadata
long FaultInjection::getFaultIndex(const Instruction *inst) {
		MDNode *mdnode = inst->getMetadata("fault_index");
		if (mdnode) {
				ConstantInt *cns_index = dyn_cast<ConstantInt>(dyn_cast<ConstantAsMetadata>(mdnode->getOperand(0))->getValue());
				return cns_index->getSExtValue();
		} else {
				return 0;
		}
}

void FaultInjection::setFaultIndex(Instruction *inst) {
	Function *func = inst->getParent()->getParent();
	LLVMContext &context = func->getContext();
	MDNode *mdnode = MDNode::get(context, ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(context), fault_count++)));
	inst->setMetadata("fault_index", mdnode);
}

//do actual fault injection
#define INF 99999
void FaultInjection::doMultiFaultInjection(Instruction* beginInstruction,int numofBits,int windowSize,Module &M){

		LLVMContext &context = M.getContext();

		//set the globalvariable for the multi bit flip
		ConstantInt *faultConstant = ConstantInt::get(Type::getInt64Ty(context),1,false);
		GlobalVariable *curFault = new GlobalVariable(M, Type::getInt64Ty(context), false,
				GlobalValue::InternalLinkage, faultConstant, "curFault",NULL,
				GlobalValue::NotThreadLocal, 0, false);

		if(windowSize==0){
			createFaultInjectionFunctionforMulti(beginInstruction->getType(),curFault,M);
			Instruction* faultInjectionCall = insertFaultCallforMulti(beginInstruction,numofBits,true,M);
			faultInjectionCall->insertAfter(beginInstruction);
			setFaultIndex(faultInjectionCall);
			for (User *U : beginInstruction->users()) {
			  if (Instruction *Inst = dyn_cast<Instruction>(U)) {
			     	if(getFaultIndex(Inst)!=getFaultIndex(faultInjectionCall)){
								Inst->replaceUsesOfWith(beginInstruction,faultInjectionCall);
						}
				}
			}

			/*for(Value::use_iterator ui= beginInstruction->use_begin(); ui != beginInstruction->use_end(); ++ui){
					User *u= cast<User>(*ui);
					u->replaceUsesOfWith(beginInstruction, faultInjectionCall);
			}*/
		}
		else{
			std::vector<faultPoint> fault_points = getTargetInstructions(beginInstruction,numofBits,windowSize);
			for(auto point : fault_points){
	        createFaultInjectionFunctionforMulti(point.instruction->getType(),curFault,M);
	        Instruction* faultInjectionCall = insertFaultCallforMulti(point.instruction,point.numofBits,false,M);
					faultInjectionCall->insertAfter(point.instruction);
					setFaultIndex(faultInjectionCall);
					for (User *U : point.instruction->users()) {
						if (Instruction *Inst = dyn_cast<Instruction>(U)) {
								if(getFaultIndex(Inst)!=getFaultIndex(faultInjectionCall)){
										Inst->replaceUsesOfWith(point.instruction,faultInjectionCall);
								}
						}
					}

	        /*for(Value::use_iterator ui= point.instruction->use_begin(); ui != point.instruction->use_end(); ++ui){
	            User *u= cast<User>(*ui);
	            u->replaceUsesOfWith(point.instruction,faultInjectionCall);
	        }*/
	    }
		}
}

Instruction* FaultInjection::insertFaultCallforMulti (Instruction* target,int numofBits, bool singleInstruction, Module &M){
    LLVMContext &context = M.getContext();
    std::map<std::string, std::string>::iterator iter;
		std::string type_str;
		llvm::raw_string_ostream rso(type_str);
		target->getType()->print(rso);
    iter = function_name_for_type.find(rso.str());
    if(iter != function_name_for_type.end()){
				std::vector<Value *> injectArgs;
				std::string function_name = iter->second;
        Function* injectFunction = M.getFunction(function_name);
				//First argument target instruction
				injectArgs.push_back(target);
				int width = target->getType()->getPrimitiveSizeInBits();
				if (width == 0){
						width = 64;
				}
				__int128 xor_value = 0;
				struct timeval time;

				if(singleInstruction){
					//Second argument "CurFault"
					injectArgs.push_back(ConstantInt::get(Type::getInt64Ty(context),1,false));
					gettimeofday(&time,NULL);
					// microsecond has 1 000 000
					// Assuming you did not need quite that accuracy
					// Also do not assume the system clock has that accuracy.
					srand((time.tv_sec * 1000) + (time.tv_usec / 1000));
					int random=rand();
					while(numofBits!=0){
						xor_value = xor_value|(1<<(random%width));
						numofBits--;
						srand(random);
						random = rand();
					}
				}else{
					//Second argument "CurFault"
        	        injectArgs.push_back(ConstantInt::get(Type::getInt64Ty(context),numofBits,false));
					gettimeofday(&time,NULL);
					// microsecond has 1 000 000
					// Assuming you did not need quite that accuracy
					// Also do not assume the system clock has that accuracy.
					srand((time.tv_sec * 1000) + (time.tv_usec / 1000));
					xor_value = 1<<(rand()%width);
				}
				Type* cast_type;
                cast_type = Type::getIntNTy(context,width);;	
                
				injectArgs.push_back(ConstantInt::get(cast_type,xor_value,false));
				IRBuilder<> builder(context);
        CallInst* callInject = builder.CreateCall(injectFunction,injectArgs);
        return callInject;
    }
    else{
        errs()<<"The fault injection function has not been created!\n";
    }
    return nullptr;
}

void FaultInjection::createFaultInjectionFunctionforMulti (Type* target_type,GlobalVariable* curFault,Module &M){
	std::map<std::string, std::string>::iterator iter;
	std::string type_str;
	llvm::raw_string_ostream rso(type_str);
	target_type->print(rso);
	type_str = rso.str();
	iter = function_name_for_type.find(type_str);

	std::string function_name;
	LLVMContext &context = M.getContext();
	bool inited = false;
	//Make sure that if the fault injection function has been inited
	if(iter != function_name_for_type.end()){
			inited = true;
			function_name = iter->second;
	}
	else{
			function_name = "faultInjectfor"+type_str;
			function_name_for_type.insert(std::pair<std::string, std::string>(type_str,function_name));
	}

	int width = target_type->getPrimitiveSizeInBits();
	if (width == 0){
			errs()<<"Undefined type!"<<"\n";
			width = 64;
	}
	Type* paramsRef[3];
	paramsRef[0] = target_type;
	paramsRef[1] = Type::getInt64Ty(context);
	Type *cast_type;
	cast_type = Type::getIntNTy(context,width);    
	paramsRef[2] = cast_type;

	ArrayRef<Type*> faultInjectParams(paramsRef);
	FunctionType* injectType = FunctionType::get(target_type, faultInjectParams, true);
	Constant* c = M.getOrInsertFunction(function_name,injectType);
	Function* injectFunction = cast<Function>(c);

	//init fault injection function
	if(!inited){
			//get the args of inject function
			std::vector<Value*> args;
			for(Function::arg_iterator ai = injectFunction->arg_begin(); ai != injectFunction->arg_end(); ++ai){
					args.push_back(&*ai);
			}
			Value* target_instruction = args[0];
			Value* set_fault = args[1];
			Value* xor_value = args[2];

			BasicBlock* entry_block = BasicBlock::Create(context,"entry",injectFunction);
			IRBuilder<> builder(M.getContext());
			builder.SetInsertPoint(entry_block);
			//get the target type width

			srand((unsigned)time(NULL));
			int target_bit = 1<<(rand()%width);
			Value* bit_flip;
			//initate bit_flip instruction for entry block
			if(target_type->isIntegerTy()){
					bit_flip = builder.CreateXor(target_instruction, xor_value, "bit_flip");
			}
			else if(target_type->isFloatingPointTy()){
					Value* int_cast = builder.CreateBitCast(target_instruction,cast_type,"cast_to_int");
					Value* cast_flip = builder.CreateXor(int_cast, xor_value, "bit_flip");
					bit_flip = builder.CreateBitCast(cast_flip,target_type,"cast_to_target");
			}
			else if(target_type->isPointerTy()){
					Type *cast_type = Type::getIntNTy(context,width);
					Value* int_cast = builder.CreatePtrToInt(target_instruction,cast_type,"cast_to_int");
					Value* cast_flip = builder.CreateXor(int_cast, xor_value, "bit_flip");
					bit_flip = builder.CreateIntToPtr(cast_flip,target_type,"cast_to_target");
			}

			BasicBlock* cmp_block = BasicBlock::Create(context,"cmp", injectFunction);
			builder.SetInsertPoint(cmp_block);
			LoadInst* load_cur_fault = builder.CreateLoad(curFault,"load_cur_fault");
			Value* comp_cur_set = builder.CreateICmpEQ(load_cur_fault,set_fault,"comp_cur_set");


			/*
			//get(insert optionally) printf function
			Type *printfParams[1];
			printfParams[0] = Type::getInt8PtrTy(M.getContext());
			ArrayRef<Type *> paramsRef(printfParams);
			FunctionType *printfType = FunctionType::get(Type::getInt32Ty(M.getContext()), paramsRef,true);

			Constant *printf = M.getOrInsertFunction("printf", printfType);
			Function* printFunction = cast<Function>(printf);
			std::vector<Value *> printArgs;
			std::string faultString = "CurFault is %d!\n";
			Value *fault_string_ptr = builder.CreateGlobalStringPtr(faultString);
			printArgs.push_back(fault_string_ptr);
			printArgs.push_back(load_cur_fault);
			builder.CreateCall(printFunction,printArgs);*/

			builder.SetInsertPoint(entry_block);
			builder.CreateBr(cmp_block);

			BasicBlock* ret_block = BasicBlock::Create(context,"ret", injectFunction);
			builder.SetInsertPoint(ret_block);
			Value* fault_increment = builder.CreateAdd(load_cur_fault,ConstantInt::get(Type::getInt64Ty(context),1,false),"fault_increment");
      builder.CreateStore(fault_increment,curFault);
			builder.CreateRet(bit_flip);
      BasicBlock* printf_block = constructPrintfBlock(ret_block,M);


      BasicBlock* no_fault_block = BasicBlock::Create(context,"no_fault", injectFunction);
			builder.SetInsertPoint(no_fault_block);
			builder.CreateRet(target_instruction);
      builder.SetInsertPoint(cmp_block);
      builder.CreateCondBr(comp_cur_set,printf_block,no_fault_block);
	}
}

std::map<std::string,std::map<std::string,shortDistance>> floydWarshall (
    std::map<std::string, std::map<std::string ,size_t>> block_distances, Function &Func)
{
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

std::vector<faultPoint> FaultInjection::getTargetInstructions(Instruction* beginInstruction,int numofBits,int windowSize){
   //vector of all fault points
    std::vector<faultPoint> fault_points;
   //the vector of fault point indexes to make sure one instruction will be injected once
    std::set<long> fault_indexes;
    //short distances of the target function graph
    std::map<std::string, std::map<std::string ,size_t>> block_distances;
    //short distance of one basic block
    std::map<std::string, size_t> distance;
    //the name of all blocks in target function
    std::map<std::string, BasicBlock*> block_names;

    BasicBlock* target_block = beginInstruction->getParent();
    std::string BB_Name = target_block->getName().str();
    Function* target_function = target_block->getParent();


    for(BasicBlock &BB : *target_function){
        distance.insert(std::pair<std::string,size_t>(BB.getName().str(),INF));
    }

    for(BasicBlock &BB : *target_function)
    {
        std::map<std::string ,size_t> temp = distance;
        for (BasicBlock *suc : successors(&BB)) {
            temp[suc->getName().str()] = suc->size();
        }
        block_distances.insert(std::pair<std::string,std::map<std::string, size_t>>(BB.getName().str(),temp));
        block_names.insert(std::pair<std::string, BasicBlock*>(BB.getName().str(),&BB));
    }

    //get the short distances within one basicblock
    std::map<std::string,std::map<std::string,shortDistance>> shortDistances = floydWarshall(block_distances,*target_function);
    std::map<std::string,shortDistance> distance_to_target = shortDistances[target_block->getName().str()];
    int within_block_distance,target_distance;
    int fault_number = 2;
		int cur_number = 2;
    bool fault_block = true;
    int left_distance = 0;

		//push_back the begin instruction
		faultPoint begin_point;
		begin_point.numofBits = 1;
		begin_point.instruction = beginInstruction;
		fault_points.push_back(begin_point);
		fault_indexes.insert(getInstructionIndex(beginInstruction));

    while(fault_number<=numofBits){
        within_block_distance = getInstructionIndex(target_block->getTerminator())-getInstructionIndex(beginInstruction);
        target_distance = (fault_number-1)*windowSize-within_block_distance;
				//No need to iterate the following basic block is distance is within the
				//taregt block
        if(target_distance<=0){
            target_distance = (fault_number-1)*windowSize;
            BasicBlock::iterator it(beginInstruction);
            while(target_distance>0){
                it++;
                target_distance--;
            }
            Instruction* faultInstruction = &*it;

						//make sure the target instruction do not be injected twice and is the support type
            if(fault_indexes.find(getInstructionIndex(faultInstruction))==fault_indexes.end()
								&&isInstructionTargetType(faultInstruction))
						{
                faultPoint fault_point;
                fault_point.instruction = faultInstruction;
                fault_point.numofBits = cur_number;
								fault_indexes.insert(getInstructionIndex(faultInstruction));
                fault_points.push_back(fault_point);
								cur_number++;
            }
            fault_number++;
            continue;
        }

				bool fault_injected = false;
        for(BasicBlock &BB : *target_function){
            shortDistance distance = distance_to_target[BB.getName().str()];
            fault_block =true;
            if(distance.distance<INF && distance.distance>=target_distance){
                for(std::string BB_name : distance.path){
                    shortDistance temp_distance = distance_to_target[BB_name];
                    if(temp_distance.distance>=target_distance&&BB_name!=BB.getName().str()
										&&BB_name!=target_block->getName().str())
										{
                        fault_block=false;
                        break;
                    }
                }
                if(fault_block){
                    target_distance = target_distance-(distance.distance-BB.size());
										target_distance = target_distance-1;
                    BasicBlock::iterator temp_it = BB.begin();
                    while(target_distance>0){
                        temp_it++;
                        target_distance--;
                    }
                    Instruction* temp_instruction = &*temp_it;
                    if(fault_indexes.find(getInstructionIndex(temp_instruction))==fault_indexes.end()
											&&isInstructionTargetType(temp_instruction)){
                        faultPoint temp_point;
                        temp_point.instruction = temp_instruction;
                        temp_point.numofBits = cur_number;
												fault_indexes.insert(getInstructionIndex(temp_instruction));
                        fault_points.push_back(temp_point);
												fault_injected = true;
                    }
                }
            }
        }
				if(fault_injected)
					cur_number++;
        fault_number++;
    }
    return fault_points;
}



bool FaultInjection::isInstructionTargetType(Instruction* instruction){
    //Data instruction
    if (instruction->isBinaryOp()||instruction->isCast()||instruction->getOpcode()==32)
        return true;
    /*
    //Branch instruction
    if (instruction->getOpcode()==2)
        return true;
    //Memory instruction
    if (instruction->getOpcode()==28)
        return true;
    */
    return false;
}

bool FaultInjection::faultInjection(Instruction &target, Module &M){
		LLVMContext &context = M.getContext();
    unsigned op_code = target.getOpcode();
    ConstantInt *contantInt = ConstantInt::get(Type::getInt64Ty(context),0,false);
    GlobalVariable *frequency = new GlobalVariable(M, Type::getInt64Ty(context), false,
	        GlobalValue::InternalLinkage,contantInt, "frequency",NULL,
	        GlobalValue::NotThreadLocal, 0, false);
    Value *elementPtrIndices[2];
    LoadInst *load_frequency = new LoadInst(frequency,"load_frequency",&target);
    BinaryOperator *frequency_increment = BinaryOperator::Create(Instruction::Add,load_frequency,
    ConstantInt::get(Type::getInt64Ty(context),1,false),"frequency_increment",&target);
    StoreInst *store_frequency = new StoreInst(frequency_increment,frequency,&target);
    ICmpInst *compareFrequency = new ICmpInst(CmpInst::ICMP_NE,frequency_increment,
    ConstantInt::get(Type::getInt16Ty(context),1,false));

    if(isInstructionTargetType(&target)){
        doDataErrorInject(target,*compareFrequency,M);
       return true;
    }
    return false;
}

Instruction* FaultInjection::insertFaultInjectionFunctionCall (Instruction* target, Instruction* cond, Module &M){
    LLVMContext &context = M.getContext();
		std::map<std::string, std::string>::iterator iter;
		std::string type_str;
		llvm::raw_string_ostream rso(type_str);
		target->getType()->print(rso);
    iter = function_name_for_type.find(rso.str());
    if(iter != function_name_for_type.end()){
        std::string function_name = iter->second;
        Function* injectFunction = M.getFunction(function_name);
        std::vector<Value *> injectArgs;
        injectArgs.push_back(target);
        injectArgs.push_back(cond);
        IRBuilder<> builder(context);
        CallInst* callInject = builder.CreateCall(injectFunction,injectArgs);
        return callInject;
    }
    else{
        errs()<<"The fault injection function has not been created!\n";
    }
    return nullptr;
}

void FaultInjection::createFaultInjectionFunction(Type* target_type,Type* cond_type ,Module &M){
		std::map<std::string, std::string>::iterator iter;
		std::string type_str;
		llvm::raw_string_ostream rso(type_str);
		target_type->print(rso);
		type_str = rso.str();
		iter = function_name_for_type.find(type_str);

    std::string function_name;
    LLVMContext &context = M.getContext();
    bool inited = false;
    //Make sure that if the fault injection function has been inited
    if(iter != function_name_for_type.end()){
        inited = true;
        function_name = iter->second;
    }
    else{
        function_name_for_type.insert(std::pair<std::string, std::string>(type_str,function_name));
    }
    Type* paramsRef[2];
    paramsRef[0] = target_type;
    paramsRef[1] = cond_type;
    ArrayRef<Type*> faultInjectParams(paramsRef);
    FunctionType* injectType = FunctionType::get(target_type, faultInjectParams, true);
    Constant* c = M.getOrInsertFunction(function_name,injectType);
    Function* injectFunction = cast<Function>(c);
    if(!inited){
        //get the args of inject function
        std::vector<Value*> args;
        for(Function::arg_iterator ai = injectFunction->arg_begin(); ai != injectFunction->arg_end(); ++ai){
            args.push_back(&*ai);
        }
        Value* target_instruction = args[0];
        Value* cond_instruction = args[1];

        BasicBlock* entry_block = BasicBlock::Create(context,"entry",injectFunction);
        IRBuilder<> builder(M.getContext());
        builder.SetInsertPoint(entry_block);
        //get the target type width
        int width = target_type->getPrimitiveSizeInBits();
        if (width == 0){
            errs()<<"Undefined type!"<<"\n";
            width = 64;
        }
        srand((unsigned)time(NULL));
        int target_bit = 1<<(rand()%width);
        Value* bit_flip;
        //initate bit_flip instruction for entry block
        if(target_type->isIntegerTy()){
            Value* bit_flip = builder.CreateXor(target_instruction, ConstantInt::get(target_type,target_bit,false), "bit_flip");
        }
        else if(target_type->isFloatingPointTy()){
            Type *cast_type;
            if (width<=64)
                cast_type = Type::getIntNTy(context,width);
            else
                cast_type = Type::getInt64Ty(context);
            Value* int_cast = builder.CreateBitCast(target_instruction,cast_type,"cast_to_int");
            Value* cast_flip = builder.CreateXor(int_cast, ConstantInt::get(cast_type,target_bit,false), "bit_flip");
            bit_flip = builder.CreateBitCast(cast_flip,target_type,"cast_to_target");
        }
        else if(target_type->isPointerTy()){
            Type *cast_type = Type::getIntNTy(context,width);
            Value* int_cast = builder.CreatePtrToInt(target_instruction,cast_type,"cast_to_int");
            Value* cast_flip = builder.CreateXor(int_cast, ConstantInt::get(cast_type,target_bit,false), "bit_flip");
            bit_flip = builder.CreateIntToPtr(cast_flip,target_type,"cast_to_target");
        }
        BasicBlock* ret_block = BasicBlock::Create(context,"ret", injectFunction);
        builder.SetInsertPoint(ret_block);
        Value* select_inst = builder.CreateSelect(cond_instruction,bit_flip,target_instruction);
        ReturnInst* return_inst = builder.CreateRet(select_inst);
        BasicBlock* printf_block = constructPrintfBlock(ret_block,M);
        builder.SetInsertPoint(entry_block);
        builder.CreateCondBr(cond_instruction,printf_block,ret_block);
    }
}

void FaultInjection::doDataErrorInject(Instruction &target,Instruction &cond,Module &M){
	Type* target_type = target.getType();
    Type* cond_type = cond.getType();
    createFaultInjectionFunction(target_type, cond_type, M);
    Instruction* callInject = insertFaultInjectionFunctionCall(&target,&cond,M);
    callInject->insertBefore(&target);
    //Replace all uses of the target instruction to the fault inject
    for(Value::use_iterator ui= target.use_begin(); ui != target.use_end(); ++ui){
        User *u= cast<User>(*ui);
        u->replaceUsesOfWith(&target,callInject);
    }
}

void FaultInjection::doMemoryErrorInject(Instruction &target,Instruction &cond,Module &M){
	BasicBlock *original_block = target.getParent();
	LLVMContext &context = original_block->getContext();
	StoreInst *target_store = cast<StoreInst>(&target);
	Value *target_value = target_store->getValueOperand();
	Value *target_address = target_store->getPointerOperand();
	Type *target_type = target_value->getType();
	int width = target_type->getPrimitiveSizeInBits();
	if (width==0)
		width=64;
	srand((unsigned)time(NULL));
    int target_bit = 1<<(rand()%width);
    Instruction *bit_flip;
    Instruction *int_cast;
    Instruction *target_cast;
    Instruction *fault_inject;
	if (target_type->isIntegerTy()){
        bit_flip = BinaryOperator::Create(Instruction::Xor,target_value,
        ConstantInt::get(target_type,target_bit,false));
        bit_flip->insertAfter(&target);
        fault_inject = new StoreInst(bit_flip,target_address,"fault_inject");
        fault_inject->insertAfter(bit_flip);
	}
	else if(target_type->isFloatingPointTy()){
	    Type *cast_type;
	    if (width<=64)
            cast_type = Type::getIntNTy(context,width);
        else
            cast_type = Type::getInt64Ty(context);
	    int_cast = new BitCastInst(target_value,cast_type,"cast_to_int");
	    int_cast->insertAfter(target_store);
	    bit_flip = BinaryOperator::Create(Instruction::Xor,int_cast,
        ConstantInt::get(cast_type,target_bit,false));
        bit_flip->insertAfter(int_cast);
        target_cast = new BitCastInst(int_cast,target_type,"cast_to_target");
        target_cast->insertAfter(bit_flip);
        fault_inject = new StoreInst(bit_flip,target_address,"fault_inject");
        fault_inject->insertAfter(target_cast);
	}
	else if(target_type->isPointerTy()){
		Type *cast_type = Type::getIntNTy(context,width);
		int_cast = new PtrToIntInst(target_value,cast_type,"cast_to_int");
		int_cast->insertAfter(&target);
		bit_flip = BinaryOperator::Create(Instruction::Xor,int_cast,
        ConstantInt::get(cast_type,target_bit,false));
        bit_flip->insertAfter(int_cast);
        target_cast =  new IntToPtrInst(bit_flip,target_type,"cast_to_target");
        target_cast->insertAfter(bit_flip);
        fault_inject = new StoreInst(bit_flip,target_address,"fault_inject");
        fault_inject->insertAfter(target_cast);
	}
    BasicBlock::iterator terminator_iterator = original_block->end();
    terminator_iterator--;
    BasicBlock *new_block = original_block->splitBasicBlock(terminator_iterator,"New_block");
    BasicBlock *printf_block=constructPrintfBlock(new_block,M);
    BranchInst *new_branch = BranchInst::Create(new_block,printf_block,&cond,original_block);
}

BasicBlock* FaultInjection::constructPrintfBlock(BasicBlock* post_block, Module &M){
	BasicBlock *printf_block = BasicBlock::Create(post_block->getContext(),"Printf_Block",post_block->getParent(),post_block);
	BranchInst *printf_branch = BranchInst::Create(post_block,printf_block);
	std::string logString = "Fault has been injected!\n";
  Constant *logFormat = ConstantDataArray::getString(M.getContext(), logString, true);
  GlobalVariable *logFormatVar = new GlobalVariable(M, logFormat->getType(), true,
	GlobalValue::InternalLinkage,logFormat, "logFormat",NULL,
	GlobalValue::NotThreadLocal, 0, false);

	//set GEP instruction params
  Value *elementPtrIndices[2];
  elementPtrIndices[0] = elementPtrIndices[1] = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0, false);
	ArrayRef<Value *> ptrIndicesRef(elementPtrIndices);
	//get(insert optionally) printf function
  Type *printfParams[1];
  printfParams[0] = Type::getInt8PtrTy(M.getContext());
  ArrayRef<Type *> paramsRef(printfParams);
  FunctionType *printfType = FunctionType::get(Type::getInt32Ty(M.getContext()), paramsRef,true);

	Constant *printf = M.getOrInsertFunction("printf", printfType);
  std::vector<Value *> printArgs;
	Type* PointeeType = cast<PointerType>(logFormatVar->getType()->getScalarType())->getElementType();
  Value *formatGetInst = GetElementPtrInst::Create(PointeeType,logFormatVar, ptrIndicesRef,"",printf_branch);
  printArgs.push_back(formatGetInst);
  CallInst::Create(printf,printArgs,"",printf_branch);
  return printf_block;
}

char FaultInjection::ID = 0;
static RegisterPass<FaultInjection> X(
    "FaultInjection", "Inject fault into target insrtuction");
}
