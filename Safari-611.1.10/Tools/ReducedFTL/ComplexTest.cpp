/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// Compile like so:
// clang++ -O3 -o ComplexTest ComplexTest.cpp -std=c++14 `~/Downloads/clang+llvm-3.7.0-x86_64-apple-darwin/bin/llvm-config --cppflags --cxxflags --ldflags --libs` -lcurses -g -lz

#include <vector>
#include <memory>
#include <chrono>

#include <sys/time.h>

#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/DataLayout.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/Pass.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/Host.h>
#include <llvm/Analysis/Passes.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/IPO.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/Support/ErrorHandling.h>
#include <llvm/Support/TargetSelect.h>

using namespace llvm;
using namespace std;

namespace {

template<typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    static_assert(sizeof(FromType) == sizeof(ToType), "bitwise_cast size of FromType and ToType must be equal!");
    union {
        FromType from;
        ToType to;
    } u;
    u.from = from;
    return u.to;
}

double monotonicallyIncreasingTimeMS()
{
    return chrono::duration_cast<chrono::microseconds>(
        chrono::steady_clock::now().time_since_epoch()).count() / 1000.;
}

void run(unsigned numVars, unsigned numConstructs)
{
    LLVMContext context;
    Type* int32Ty = Type::getInt32Ty(context);
    Module* module = new Module("complexModule", context);
    Function* function = Function::Create(
        FunctionType::get(int32Ty, false), GlobalValue::ExternalLinkage, "complexFunction", module);

    vector<int32_t> varSlots;
    for (unsigned i = numVars; i--;)
        varSlots.push_back(i);

    BasicBlock* current = BasicBlock::Create(context, "", function);
    unique_ptr<IRBuilder<>> builder = make_unique<IRBuilder<>>(current);

    vector<Value*> vars;
    for (int32_t& varSlot : varSlots) {
        Value* ptr = builder->CreateIntToPtr(builder->getInt64(bitwise_cast<intptr_t>(&varSlot)),
                                            int32Ty->getPointerTo());
        vars.push_back(builder->CreateLoad(ptr));
    }

    for (unsigned i = 0; i < numConstructs; ++i) {
        if (i & 1) {
            // Control flow diamond.
            unsigned predicateVarIndex = ((i >> 1) + 2) % numVars;
            unsigned thenIncVarIndex = ((i >> 1) + 0) % numVars;
            unsigned elseIncVarIndex = ((i >> 1) + 1) % numVars;

            BasicBlock* thenBlock = BasicBlock::Create(context, "", function);
            BasicBlock* elseBlock = BasicBlock::Create(context, "", function);
            BasicBlock* continuation = BasicBlock::Create(context, "", function);

            builder->CreateCondBr(
                builder->CreateICmpNE(vars[predicateVarIndex], builder->getInt32(0)),
                thenBlock, elseBlock);

            builder = make_unique<IRBuilder<>>(thenBlock);
            Value* thenValue = builder->CreateAdd(vars[thenIncVarIndex], builder->getInt32(1));
            builder->CreateBr(continuation);

            builder = make_unique<IRBuilder<>>(elseBlock);
            Value* elseValue = builder->CreateAdd(vars[elseIncVarIndex], builder->getInt32(1));
            builder->CreateBr(continuation);

            builder = make_unique<IRBuilder<>>(current = continuation);
            PHINode* thenPhi = builder->CreatePHI(int32Ty, 2);
            thenPhi->addIncoming(thenValue, thenBlock);
            thenPhi->addIncoming(vars[thenIncVarIndex], elseBlock);
            PHINode* elsePhi = builder->CreatePHI(int32Ty, 2);
            elsePhi->addIncoming(elseValue, elseBlock);
            elsePhi->addIncoming(vars[elseIncVarIndex], thenBlock);
            
            vars[thenIncVarIndex] = thenPhi;
            vars[elseIncVarIndex] = elsePhi;
        } else {
            // Loop.

            BasicBlock* loopBody = BasicBlock::Create(context, "", function);
            BasicBlock* continuation = BasicBlock::Create(context, "", function);

            Value* startIndex = vars[((i >> 1) + 1) % numVars];
            Value* startSum = builder->getInt32(0);
            builder->CreateCondBr(
                builder->CreateICmpNE(startIndex, builder->getInt32(0)),
                loopBody, continuation);

            builder = make_unique<IRBuilder<>>(loopBody);
            PHINode* bodyIndex = builder->CreatePHI(int32Ty, 2);
            PHINode* bodySum = builder->CreatePHI(int32Ty, 2);
            bodyIndex->addIncoming(startIndex, current);
            bodySum->addIncoming(startSum, current);

            Value* newBodyIndex = builder->CreateSub(bodyIndex, builder->getInt32(1));
            Value* newBodySum = builder->CreateAdd(
                bodySum,
                builder->CreateLoad(
                    builder->CreateIntToPtr(
                        builder->CreateAdd(
                            builder->getInt64(bitwise_cast<intptr_t>(&varSlots[0])),
                            builder->CreateShl(
                                builder->CreateZExt(
                                    builder->CreateAnd(
                                        newBodyIndex,
                                        builder->getInt32(numVars - 1)),
                                    Type::getInt64Ty(context)),
                                builder->getInt64(2))),
                        int32Ty->getPointerTo())));
            bodyIndex->addIncoming(newBodyIndex, loopBody);
            bodySum->addIncoming(newBodySum, loopBody);
            builder->CreateCondBr(
                builder->CreateICmpNE(newBodyIndex, builder->getInt32(0)),
                loopBody, continuation);

            builder = make_unique<IRBuilder<>>(continuation);
            PHINode* finalSum = builder->CreatePHI(int32Ty, 2);
            finalSum->addIncoming(startSum, current);
            finalSum->addIncoming(newBodySum, loopBody);
            current = continuation;
            vars[((i >> 1) + 0) % numVars] = finalSum;
        }
    }

    builder->CreateRet(vars[0]);
    builder = nullptr;

    unique_ptr<ExecutionEngine> EE;
    {
        std::string errorMessage;
        EngineBuilder builder((std::unique_ptr<Module>(module)));
        builder.setMArch("");
        builder.setMCPU(sys::getHostCPUName());
        builder.setMAttrs(std::vector<std::string>());
        builder.setRelocationModel(Reloc::Default);
        builder.setCodeModel(CodeModel::JITDefault);
        builder.setErrorStr(&errorMessage);
        builder.setEngineKind(EngineKind::JIT);

        builder.setOptLevel(CodeGenOpt::Default);
        
        {
            TargetOptions Options;
            Options.FloatABIType = FloatABI::Default;

            builder.setTargetOptions(Options);
        }

        EE = unique_ptr<ExecutionEngine>(builder.create());
    }
    
    legacy::PassManager passManager;
    //passManager.add(new DataLayout(*EE->getDataLayout()));
    passManager.add(createPromoteMemoryToRegisterPass());
    passManager.add(createConstantPropagationPass());
    passManager.add(createInstructionCombiningPass());
    passManager.add(createBasicAliasAnalysisPass());
    passManager.add(createTypeBasedAliasAnalysisPass());
    passManager.add(createGVNPass());
    passManager.add(createCFGSimplificationPass());
    passManager.run(*module);

    EE->getFunctionAddress(function->getName());
}

} // anonymous namespace

int main(int c, char** v)
{
    InitializeAllTargets();
    InitializeAllTargetMCs();
    InitializeAllAsmPrinters();
    InitializeAllAsmParsers();
    for (unsigned i = 1; i <= 3; ++i) {
        printf("Doing: run(4, %u)\n", i * 128);
        double before = monotonicallyIncreasingTimeMS();
        run(4, 128 * i);
        double after = monotonicallyIncreasingTimeMS();
        printf("That took %lf ms.\n", after - before);
    }
    for (unsigned i = 1; i <= 3; ++i) {
        printf("Doing: run(64, %u)\n", i * 128);
        double before = monotonicallyIncreasingTimeMS();
        run(64, 128 * i);
        double after = monotonicallyIncreasingTimeMS();
        printf("That took %lf ms.\n", after - before);
    }
    return 0;
}

