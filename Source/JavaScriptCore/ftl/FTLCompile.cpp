/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FTLCompile.h"

#if ENABLE(FTL_JIT)

#include "CodeBlockWithJITType.h"
#include "CCallHelpers.h"
#include "DFGCommon.h"
#include "DFGGraphSafepoint.h"
#include "DataView.h"
#include "Disassembler.h"
#include "FTLExitThunkGenerator.h"
#include "FTLInlineCacheSize.h"
#include "FTLJITCode.h"
#include "FTLThunks.h"
#include "FTLUnwindInfo.h"
#include "JITStubs.h"
#include "LLVMAPI.h"
#include "LinkBuffer.h"
#include "RepatchBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

static uint8_t* mmAllocateCodeSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned, const char* sectionName)
{
    State& state = *static_cast<State*>(opaqueState);
    
    RELEASE_ASSERT(alignment <= jitAllocationGranule);
    
    RefPtr<ExecutableMemoryHandle> result =
        state.graph.m_vm.executableAllocator.allocate(
            state.graph.m_vm, size, state.graph.m_codeBlock, JITCompilationMustSucceed);
    
    // LLVM used to put __compact_unwind in a code section. We keep this here defensively,
    // for clients that use older LLVMs.
    if (!strcmp(sectionName, "__compact_unwind")) {
        state.compactUnwind = result->start();
        state.compactUnwindSize = result->sizeInBytes();
    }
    
    state.jitCode->addHandle(result);
    state.codeSectionNames.append(sectionName);
    
    return static_cast<uint8_t*>(result->start());
}

static uint8_t* mmAllocateDataSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID,
    const char* sectionName, LLVMBool isReadOnly)
{
    UNUSED_PARAM(sectionID);
    UNUSED_PARAM(isReadOnly);

    State& state = *static_cast<State*>(opaqueState);
    
    RefPtr<DataSection> section = adoptRef(new DataSection(
        state.graph.m_vm, state.graph.m_codeBlock, size, alignment));
    
    if (!strcmp(sectionName, "__llvm_stackmaps"))
        state.stackmapsSection = section;
    else {
        state.jitCode->addDataSection(section);
        state.dataSectionNames.append(sectionName);
        if (!strcmp(sectionName, "__compact_unwind")) {
            state.compactUnwind = section->base();
            state.compactUnwindSize = size;
        }
    }
    
    return bitwise_cast<uint8_t*>(section->base());
}

static LLVMBool mmApplyPermissions(void*, char**)
{
    return false;
}

static void mmDestroy(void*)
{
}

static void dumpDataSection(DataSection* section, const char* prefix)
{
    for (unsigned j = 0; j < section->size() / sizeof(int64_t); ++j) {
        char buf[32];
        int64_t* wordPointer = static_cast<int64_t*>(section->base()) + j;
        snprintf(buf, sizeof(buf), "0x%lx", static_cast<unsigned long>(bitwise_cast<uintptr_t>(wordPointer)));
        dataLogF("%s%16s: 0x%016llx\n", prefix, buf, static_cast<long long>(*wordPointer));
    }
}

template<typename DescriptorType>
void generateICFastPath(
    State& state, CodeBlock* codeBlock, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, DescriptorType& ic, size_t sizeOfIC)
{
    VM& vm = state.graph.m_vm;

    StackMaps::RecordMap::iterator iter = recordMap.find(ic.stackmapID());
    if (iter == recordMap.end()) {
        // It was optimized out.
        return;
    }
    
    Vector<StackMaps::Record>& records = iter->value;
    
    RELEASE_ASSERT(records.size() == ic.m_generators.size());
    
    for (unsigned i = records.size(); i--;) {
        StackMaps::Record& record = records[i];
        auto generator = ic.m_generators[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        generator.generateFastPath(fastPathJIT);
        
        char* startOfIC =
            bitwise_cast<char*>(generatedFunction) + record.instructionOffset;
        
        LinkBuffer linkBuffer(vm, &fastPathJIT, startOfIC, sizeOfIC);
        // Note: we could handle the !isValid() case. We just don't appear to have a
        // reason to do so, yet.
        RELEASE_ASSERT(linkBuffer.isValid());
        
        MacroAssembler::AssemblerType_T::fillNops(
            startOfIC + linkBuffer.size(), sizeOfIC - linkBuffer.size());
        
        state.finalizer->sideCodeLinkBuffer->link(
            ic.m_slowPathDone[i], CodeLocationLabel(startOfIC + sizeOfIC));
        
        linkBuffer.link(
            generator.slowPathJump(),
            state.finalizer->sideCodeLinkBuffer->locationOf(generator.slowPathBegin()));
        
        generator.finalize(linkBuffer, *state.finalizer->sideCodeLinkBuffer);
    }
}

static void fixFunctionBasedOnStackMaps(
    State& state, CodeBlock* codeBlock, JITCode* jitCode, GeneratedFunction generatedFunction,
    StackMaps::RecordMap& recordMap, bool didSeeUnwindInfo)
{
    Graph& graph = state.graph;
    VM& vm = graph.m_vm;
    StackMaps stackmaps = jitCode->stackmaps;
    
    StackMaps::RecordMap::iterator iter = recordMap.find(state.capturedStackmapID);
    RELEASE_ASSERT(iter != recordMap.end());
    RELEASE_ASSERT(iter->value.size() == 1);
    RELEASE_ASSERT(iter->value[0].locations.size() == 1);
    Location capturedLocation =
        Location::forStackmaps(&jitCode->stackmaps, iter->value[0].locations[0]);
    RELEASE_ASSERT(capturedLocation.kind() == Location::Register);
    RELEASE_ASSERT(capturedLocation.gpr() == GPRInfo::callFrameRegister);
    RELEASE_ASSERT(!(capturedLocation.addend() % sizeof(Register)));
    int32_t localsOffset = capturedLocation.addend() / sizeof(Register) + graph.m_nextMachineLocal;
    
    for (unsigned i = graph.m_inlineVariableData.size(); i--;) {
        InlineCallFrame* inlineCallFrame = graph.m_inlineVariableData[i].inlineCallFrame;
        
        if (inlineCallFrame->argumentsRegister.isValid()) {
            inlineCallFrame->argumentsRegister = VirtualRegister(
                inlineCallFrame->argumentsRegister.offset() + localsOffset);
        }
        
        for (unsigned argument = inlineCallFrame->arguments.size(); argument-- > 1;) {
            inlineCallFrame->arguments[argument] =
                inlineCallFrame->arguments[argument].withLocalsOffset(localsOffset);
        }
        
        if (inlineCallFrame->isClosureCall) {
            inlineCallFrame->calleeRecovery =
                inlineCallFrame->calleeRecovery.withLocalsOffset(localsOffset);
        }
    }
    
    if (codeBlock->usesArguments()) {
        codeBlock->setArgumentsRegister(
            VirtualRegister(codeBlock->argumentsRegister().offset() + localsOffset));
    }

    MacroAssembler::Label stackOverflowException;

    {
        CCallHelpers checkJIT(&vm, codeBlock);
        
        // At this point it's perfectly fair to just blow away all state and restore the
        // JS JIT view of the universe.
        checkJIT.move(GPRInfo::callFrameRegister, GPRInfo::argumentGPR1);

        MacroAssembler::Label exceptionContinueArg1Set = checkJIT.label();
        checkJIT.move(MacroAssembler::TrustedImm64(TagTypeNumber), GPRInfo::tagTypeNumberRegister);
        checkJIT.move(MacroAssembler::TrustedImm64(TagMask), GPRInfo::tagMaskRegister);

        checkJIT.move(MacroAssembler::TrustedImmPtr(&vm), GPRInfo::argumentGPR0);
        MacroAssembler::Call call = checkJIT.call();
        checkJIT.jumpToExceptionHandler();

        stackOverflowException = checkJIT.label();
        checkJIT.emitGetCallerFrameFromCallFrameHeaderPtr(GPRInfo::argumentGPR1);
        checkJIT.jump(exceptionContinueArg1Set);

        OwnPtr<LinkBuffer> linkBuffer = adoptPtr(new LinkBuffer(
            vm, &checkJIT, codeBlock, JITCompilationMustSucceed));
        linkBuffer->link(call, FunctionPtr(lookupExceptionHandler));
        
        state.finalizer->handleExceptionsLinkBuffer = linkBuffer.release();
    }

    ExitThunkGenerator exitThunkGenerator(state);
    exitThunkGenerator.emitThunks();
    if (exitThunkGenerator.didThings()) {
        RELEASE_ASSERT(state.finalizer->osrExit.size());
        RELEASE_ASSERT(didSeeUnwindInfo);
        
        OwnPtr<LinkBuffer> linkBuffer = adoptPtr(new LinkBuffer(
            vm, &exitThunkGenerator, codeBlock, JITCompilationMustSucceed));
        
        RELEASE_ASSERT(state.finalizer->osrExit.size() == state.jitCode->osrExit.size());
        
        for (unsigned i = 0; i < state.jitCode->osrExit.size(); ++i) {
            OSRExitCompilationInfo& info = state.finalizer->osrExit[i];
            OSRExit& exit = jitCode->osrExit[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling OSR stackmap #", exit.m_stackmapID, " for ", exit.m_codeOrigin, "\n");

            iter = recordMap.find(exit.m_stackmapID);
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            info.m_thunkAddress = linkBuffer->locationOf(info.m_thunkLabel);
            exit.m_patchableCodeOffset = linkBuffer->offsetOf(info.m_thunkJump);
            
            for (unsigned j = exit.m_values.size(); j--;) {
                ExitValue value = exit.m_values[j];
                if (!value.isInJSStackSomehow())
                    continue;
                if (!value.virtualRegister().isLocal())
                    continue;
                exit.m_values[j] = value.withVirtualRegister(
                    VirtualRegister(value.virtualRegister().offset() + localsOffset));
            }
            
            if (verboseCompilationEnabled()) {
                DumpContext context;
                dataLog("    Exit values: ", inContext(exit.m_values, &context), "\n");
            }
        }
        
        state.finalizer->exitThunksLinkBuffer = linkBuffer.release();
    }

    if (!state.getByIds.isEmpty() || !state.putByIds.isEmpty()) {
        CCallHelpers slowPathJIT(&vm, codeBlock);
        
        CCallHelpers::JumpList exceptionTarget;
        
        for (unsigned i = state.getByIds.size(); i--;) {
            GetByIdDescriptor& getById = state.getByIds[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling GetById stackmap #", getById.stackmapID(), "\n");
            
            iter = recordMap.find(getById.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i];
            
                // FIXME: LLVM should tell us which registers are live.
                RegisterSet usedRegisters = RegisterSet::allRegisters();
                
                GPRReg result = record.locations[0].directGPR();
                GPRReg base = record.locations[1].directGPR();
                
                JITGetByIdGenerator gen(
                    codeBlock, getById.codeOrigin(), usedRegisters, JSValueRegs(base),
                    JSValueRegs(result), NeedToSpill);
                
                MacroAssembler::Label begin = slowPathJIT.label();
                
                MacroAssembler::Call call = callOperation(
                    state, usedRegisters, slowPathJIT, getById.codeOrigin(), &exceptionTarget,
                    operationGetByIdOptimize, result, gen.stubInfo(), base, getById.uid());
                
                gen.reportSlowPathCall(begin, call);
                
                getById.m_slowPathDone.append(slowPathJIT.jump());
                getById.m_generators.append(gen);
            }
        }
        
        for (unsigned i = state.putByIds.size(); i--;) {
            PutByIdDescriptor& putById = state.putByIds[i];
            
            if (verboseCompilationEnabled())
                dataLog("Handling PutById stackmap #", putById.stackmapID(), "\n");
            
            iter = recordMap.find(putById.stackmapID());
            if (iter == recordMap.end()) {
                // It was optimized out.
                continue;
            }
            
            for (unsigned i = 0; i < iter->value.size(); ++i) {
                StackMaps::Record& record = iter->value[i];
                
                // FIXME: LLVM should tell us which registers are live.
                RegisterSet usedRegisters = RegisterSet::allRegisters();
                
                GPRReg base = record.locations[0].directGPR();
                GPRReg value = record.locations[1].directGPR();
                
                JITPutByIdGenerator gen(
                    codeBlock, putById.codeOrigin(), usedRegisters, JSValueRegs(base),
                    JSValueRegs(value), GPRInfo::patchpointScratchRegister, NeedToSpill,
                    putById.ecmaMode(), putById.putKind());
                
                MacroAssembler::Label begin = slowPathJIT.label();
                
                MacroAssembler::Call call = callOperation(
                    state, usedRegisters, slowPathJIT, putById.codeOrigin(), &exceptionTarget,
                    gen.slowPathFunction(), gen.stubInfo(), value, base, putById.uid());
                
                gen.reportSlowPathCall(begin, call);
                
                putById.m_slowPathDone.append(slowPathJIT.jump());
                putById.m_generators.append(gen);
            }
        }
        
        exceptionTarget.link(&slowPathJIT);
        MacroAssembler::Jump exceptionJump = slowPathJIT.jump();
        
        state.finalizer->sideCodeLinkBuffer = adoptPtr(
            new LinkBuffer(vm, &slowPathJIT, codeBlock, JITCompilationMustSucceed));
        state.finalizer->sideCodeLinkBuffer->link(
            exceptionJump, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        
        for (unsigned i = state.getByIds.size(); i--;) {
            generateICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.getByIds[i],
                sizeOfGetById());
        }
        for (unsigned i = state.putByIds.size(); i--;) {
            generateICFastPath(
                state, codeBlock, generatedFunction, recordMap, state.putByIds[i],
                sizeOfPutById());
        }
    }
    
    // Handling JS calls is weird: we need to ensure that we sort them by the PC in LLVM
    // generated code. That implies first pruning the ones that LLVM didn't generate.
    Vector<JSCall> oldCalls = state.jsCalls;
    state.jsCalls.resize(0);
    for (unsigned i = 0; i < oldCalls.size(); ++i) {
        JSCall& call = oldCalls[i];
        
        StackMaps::RecordMap::iterator iter = recordMap.find(call.stackmapID());
        if (iter == recordMap.end())
            continue;

        for (unsigned j = 0; j < iter->value.size(); ++j) {
            JSCall copy = call;
            copy.m_instructionOffset = iter->value[j].instructionOffset;
            state.jsCalls.append(copy);
        }
    }
    
    std::sort(state.jsCalls.begin(), state.jsCalls.end());
    
    for (unsigned i = state.jsCalls.size(); i--;) {
        JSCall& call = state.jsCalls[i];

        CCallHelpers fastPathJIT(&vm, codeBlock);
        call.emit(fastPathJIT);
        
        char* startOfIC = bitwise_cast<char*>(generatedFunction) + call.m_instructionOffset;
        
        LinkBuffer linkBuffer(vm, &fastPathJIT, startOfIC, sizeOfCall());
        if (!linkBuffer.isValid()) {
            dataLog("Failed to insert inline cache for call because we thought the size would be ", sizeOfCall(), " but it ended up being ", fastPathJIT.m_assembler.codeSize(), " prior to compaction.\n");
            RELEASE_ASSERT_NOT_REACHED();
        }
        
        MacroAssembler::AssemblerType_T::fillNops(
            startOfIC + linkBuffer.size(), sizeOfCall() - linkBuffer.size());
        
        call.link(vm, linkBuffer);
    }
    
    RepatchBuffer repatchBuffer(codeBlock);

    iter = recordMap.find(state.handleStackOverflowExceptionStackmapID);
    // It's sort of remotely possible that we won't have an in-band exception handling
    // path, for some kinds of functions.
    if (iter != recordMap.end()) {
        for (unsigned i = iter->value.size(); i--;) {
            StackMaps::Record& record = iter->value[i];
            
            CodeLocationLabel source = CodeLocationLabel(
                bitwise_cast<char*>(generatedFunction) + record.instructionOffset);

            RELEASE_ASSERT(stackOverflowException.isSet());

            repatchBuffer.replaceWithJump(source, state.finalizer->handleExceptionsLinkBuffer->locationOf(stackOverflowException));
        }
    }
    
    iter = recordMap.find(state.handleExceptionStackmapID);
    // It's sort of remotely possible that we won't have an in-band exception handling
    // path, for some kinds of functions.
    if (iter != recordMap.end()) {
        for (unsigned i = iter->value.size(); i--;) {
            StackMaps::Record& record = iter->value[i];
            
            CodeLocationLabel source = CodeLocationLabel(
                bitwise_cast<char*>(generatedFunction) + record.instructionOffset);
            
            repatchBuffer.replaceWithJump(source, state.finalizer->handleExceptionsLinkBuffer->entrypoint());
        }
    }
    
    for (unsigned exitIndex = 0; exitIndex < jitCode->osrExit.size(); ++exitIndex) {
        OSRExitCompilationInfo& info = state.finalizer->osrExit[exitIndex];
        OSRExit& exit = jitCode->osrExit[exitIndex];
        iter = recordMap.find(exit.m_stackmapID);
        
        Vector<const void*> codeAddresses;
        
        if (iter != recordMap.end()) {
            for (unsigned i = iter->value.size(); i--;) {
                StackMaps::Record& record = iter->value[i];
                
                CodeLocationLabel source = CodeLocationLabel(
                    bitwise_cast<char*>(generatedFunction) + record.instructionOffset);
                
                codeAddresses.append(bitwise_cast<char*>(generatedFunction) + record.instructionOffset + MacroAssembler::maxJumpReplacementSize());
                
                if (info.m_isInvalidationPoint)
                    jitCode->common.jumpReplacements.append(JumpReplacement(source, info.m_thunkAddress));
                else
                    repatchBuffer.replaceWithJump(source, info.m_thunkAddress);
            }
        }
        
        if (graph.compilation())
            graph.compilation()->addOSRExitSite(codeAddresses);
    }
}

void compile(State& state)
{
    char* error = 0;
    
    {
        GraphSafepoint safepoint(state.graph);
        
        LLVMMCJITCompilerOptions options;
        llvm->InitializeMCJITCompilerOptions(&options, sizeof(options));
        options.OptLevel = Options::llvmBackendOptimizationLevel();
        options.NoFramePointerElim = true;
        if (Options::useLLVMSmallCodeModel())
            options.CodeModel = LLVMCodeModelSmall;
        options.EnableFastISel = Options::enableLLVMFastISel();
        options.MCJMM = llvm->CreateSimpleMCJITMemoryManager(
            &state, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    
        LLVMExecutionEngineRef engine;
        
        if (isARM64())
            llvm->SetTarget(state.module, "arm64-apple-ios");
        
        if (llvm->CreateMCJITCompilerForModule(&engine, state.module, &options, sizeof(options), &error)) {
            dataLog("FATAL: Could not create LLVM execution engine: ", error, "\n");
            CRASH();
        }

        LLVMPassManagerRef functionPasses = 0;
        LLVMPassManagerRef modulePasses;
    
        if (Options::llvmSimpleOpt()) {
            modulePasses = llvm->CreatePassManager();
            llvm->AddTargetData(llvm->GetExecutionEngineTargetData(engine), modulePasses);
            llvm->AddPromoteMemoryToRegisterPass(modulePasses);
            llvm->AddConstantPropagationPass(modulePasses);
            llvm->AddInstructionCombiningPass(modulePasses);
            llvm->AddBasicAliasAnalysisPass(modulePasses);
            llvm->AddTypeBasedAliasAnalysisPass(modulePasses);
            llvm->AddGVNPass(modulePasses);
            llvm->AddCFGSimplificationPass(modulePasses);
            llvm->AddDeadStoreEliminationPass(modulePasses);
            llvm->RunPassManager(modulePasses, state.module);
        } else {
            LLVMPassManagerBuilderRef passBuilder = llvm->PassManagerBuilderCreate();
            llvm->PassManagerBuilderSetOptLevel(passBuilder, Options::llvmOptimizationLevel());
            llvm->PassManagerBuilderSetSizeLevel(passBuilder, Options::llvmSizeLevel());
        
            functionPasses = llvm->CreateFunctionPassManagerForModule(state.module);
            modulePasses = llvm->CreatePassManager();
        
            llvm->AddTargetData(llvm->GetExecutionEngineTargetData(engine), modulePasses);
        
            llvm->PassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
            llvm->PassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);
        
            llvm->PassManagerBuilderDispose(passBuilder);
        
            llvm->InitializeFunctionPassManager(functionPasses);
            for (LValue function = llvm->GetFirstFunction(state.module); function; function = llvm->GetNextFunction(function))
                llvm->RunFunctionPassManager(functionPasses, function);
            llvm->FinalizeFunctionPassManager(functionPasses);
        
            llvm->RunPassManager(modulePasses, state.module);
        }

        if (shouldShowDisassembly() || verboseCompilationEnabled())
            state.dumpState("after optimization");
    
        // FIXME: Need to add support for the case where JIT memory allocation failed.
        // https://bugs.webkit.org/show_bug.cgi?id=113620
        state.generatedFunction = reinterpret_cast<GeneratedFunction>(llvm->GetPointerToGlobal(engine, state.function));
        if (functionPasses)
            llvm->DisposePassManager(functionPasses);
        llvm->DisposePassManager(modulePasses);
        llvm->DisposeExecutionEngine(engine);
    }

    if (shouldShowDisassembly()) {
        for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
            ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
            dataLog(
                "Generated LLVM code for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                " #", i, ", ", state.codeSectionNames[i], ":\n");
            disassemble(
                MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                "    ", WTF::dataFile(), LLVMSubset);
        }
        
        for (unsigned i = 0; i < state.jitCode->dataSections().size(); ++i) {
            DataSection* section = state.jitCode->dataSections()[i].get();
            dataLog(
                "Generated LLVM data section for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                " #", i, ", ", state.dataSectionNames[i], ":\n");
            dumpDataSection(section, "    ");
        }
    }
    
    bool didSeeUnwindInfo = state.jitCode->unwindInfo.parse(
        state.compactUnwind, state.compactUnwindSize, state.generatedFunction);
    if (shouldShowDisassembly()) {
        dataLog("Unwind info for ", CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT), ":\n");
        if (didSeeUnwindInfo)
            dataLog("    ", state.jitCode->unwindInfo, "\n");
        else
            dataLog("    <no unwind info>\n");
    }
    
    if (state.stackmapsSection && state.stackmapsSection->size()) {
        if (shouldShowDisassembly()) {
            dataLog(
                "Generated LLVM stackmaps section for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT), ":\n");
            dataLog("    Raw data:\n");
            dumpDataSection(state.stackmapsSection.get(), "    ");
        }
        
        RefPtr<DataView> stackmapsData = DataView::create(
            ArrayBuffer::create(state.stackmapsSection->base(), state.stackmapsSection->size()));
        state.jitCode->stackmaps.parse(stackmapsData.get());
    
        if (shouldShowDisassembly()) {
            dataLog("    Structured data:\n");
            state.jitCode->stackmaps.dumpMultiline(WTF::dataFile(), "        ");
        }
        
        StackMaps::RecordMap recordMap = state.jitCode->stackmaps.computeRecordMap();
        fixFunctionBasedOnStackMaps(
            state, state.graph.m_codeBlock, state.jitCode.get(), state.generatedFunction,
            recordMap, didSeeUnwindInfo);
        
        if (shouldShowDisassembly()) {
            for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
                if (state.codeSectionNames[i] != "__text")
                    continue;
                
                ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
                dataLog(
                    "Generated LLVM code after stackmap-based fix-up for ",
                    CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::FTLJIT),
                    " in ", state.graph.m_plan.mode, " #", i, ", ",
                    state.codeSectionNames[i], ":\n");
                disassemble(
                    MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                    "    ", WTF::dataFile(), LLVMSubset);
            }
        }
    }
    
    state.module = 0; // We no longer own the module.
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

