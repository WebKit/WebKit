/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "Disassembler.h"
#include "FTLJITCode.h"
#include "JITStubs.h"
#include "LinkBuffer.h"
#include <wtf/LLVMHeaders.h>

namespace JSC { namespace FTL {

using namespace DFG;

static uint8_t* mmAllocateCodeSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID)
{
    UNUSED_PARAM(sectionID);
    
    State& state = *static_cast<State*>(opaqueState);
    
    RELEASE_ASSERT(alignment <= jitAllocationGranule);
    
    RefPtr<ExecutableMemoryHandle> result =
        state.graph.m_vm.executableAllocator.allocate(
            state.graph.m_vm, size, state.graph.m_codeBlock, JITCompilationMustSucceed);
    
    state.jitCode->addHandle(result);
    
    return static_cast<uint8_t*>(result->start());
}

static uint8_t* mmAllocateDataSection(
    void* opaqueState, uintptr_t size, unsigned alignment, unsigned sectionID,
    LLVMBool isReadOnly)
{
    // FIXME: fourthTier: FTL memory allocator should be able to allocate data
    // sections in non-executable memory.
    // https://bugs.webkit.org/show_bug.cgi?id=116189
    UNUSED_PARAM(isReadOnly);
    return mmAllocateCodeSection(opaqueState, size, alignment, sectionID);
}

static LLVMBool mmApplyPermissions(void*, char**)
{
    return false;
}

static void mmDestroy(void*)
{
}

void compile(State& state)
{
    char* error = 0;
    
    LLVMMCJITCompilerOptions options;
    LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel = Options::llvmBackendOptimizationLevel();
    options.NoFramePointerElim = true;
    if (Options::useLLVMSmallCodeModel())
        options.CodeModel = LLVMCodeModelSmall;
    options.EnableFastISel = Options::enableLLVMFastISel();
    options.MCJMM = LLVMCreateSimpleMCJITMemoryManager(
        &state, mmAllocateCodeSection, mmAllocateDataSection, mmApplyPermissions, mmDestroy);
    
    LLVMExecutionEngineRef engine;
    
    if (LLVMCreateMCJITCompilerForModule(&engine, state.module, &options, sizeof(options), &error)) {
        dataLog("FATAL: Could not create LLVM execution engine: ", error, "\n");
        CRASH();
    }

    LLVMPassManagerBuilderRef passBuilder = LLVMPassManagerBuilderCreate();
    LLVMPassManagerBuilderSetOptLevel(passBuilder, Options::llvmOptimizationLevel());
    LLVMPassManagerBuilderSetSizeLevel(passBuilder, Options::llvmSizeLevel());
    
    LLVMPassManagerRef functionPasses = LLVMCreateFunctionPassManagerForModule(state.module);
    LLVMPassManagerRef modulePasses = LLVMCreatePassManager();
    
    LLVMAddTargetData(LLVMGetExecutionEngineTargetData(engine), modulePasses);
    
    LLVMPassManagerBuilderPopulateFunctionPassManager(passBuilder, functionPasses);
    LLVMPassManagerBuilderPopulateModulePassManager(passBuilder, modulePasses);
    
    LLVMPassManagerBuilderDispose(passBuilder);

    LLVMInitializeFunctionPassManager(functionPasses);
    for (LValue function = LLVMGetFirstFunction(state.module); function; function = LLVMGetNextFunction(function))
        LLVMRunFunctionPassManager(functionPasses, function);
    LLVMFinalizeFunctionPassManager(functionPasses);
    
    LLVMRunPassManager(modulePasses, state.module);

    if (DFG::shouldShowDisassembly() || DFG::verboseCompilationEnabled())
        state.dumpState("after optimization");
    
    // FIXME: Need to add support for the case where JIT memory allocation failed.
    // https://bugs.webkit.org/show_bug.cgi?id=113620
    state.generatedFunction = reinterpret_cast<GeneratedFunction>(LLVMGetPointerToGlobal(engine, state.function));
    LLVMDisposePassManager(functionPasses);
    LLVMDisposePassManager(modulePasses);
    LLVMDisposeExecutionEngine(engine);

    if (shouldShowDisassembly()) {
        // FIXME: fourthTier: FTL memory allocator should be able to tell us which of
        // these things is actually code or data.
        // https://bugs.webkit.org/show_bug.cgi?id=116189
        for (unsigned i = 0; i < state.jitCode->handles().size(); ++i) {
            ExecutableMemoryHandle* handle = state.jitCode->handles()[i].get();
            dataLog(
                "Generated LLVM code for ",
                CodeBlockWithJITType(state.graph.m_codeBlock, JITCode::DFGJIT),
                " #", i, ":\n");
            disassemble(
                MacroAssemblerCodePtr(handle->start()), handle->sizeInBytes(),
                "    ", WTF::dataFile(), LLVMSubset);
        }
    }
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

