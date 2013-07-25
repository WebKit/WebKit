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
#include "DFGCCallHelpers.h"
#include "DFGCommon.h"
#include "FTLLLVMHeaders.h"
#include "JITStubs.h"
#include "LinkBuffer.h"

namespace JSC { namespace FTL {

using namespace DFG;

void compile(State& state)
{
    char* error = 0;
    
    LLVMMCJITCompilerOptions options;
    LLVMInitializeMCJITCompilerOptions(&options, sizeof(options));
    options.OptLevel = Options::llvmOptimizationLevel();
    options.NoFramePointerElim = true;
    options.CodeModel = LLVMCodeModelSmall;
    options.EnableFastISel = Options::enableLLVMFastISel();
    
    if (LLVMCreateMCJITCompilerForModule(&state.engine, state.module, &options, sizeof(options), &error)) {
        dataLog("FATAL: Could not create LLVM execution engine: ", error, "\n");
        CRASH();
    }
    
    LLVMPassManagerRef pass = LLVMCreatePassManager();
    LLVMAddTargetData(LLVMGetExecutionEngineTargetData(state.engine), pass);
    LLVMAddConstantPropagationPass(pass);
    LLVMAddInstructionCombiningPass(pass);
    LLVMAddPromoteMemoryToRegisterPass(pass);
    if (Options::enableLLVMLICM())
        LLVMAddLICMPass(pass);
    LLVMAddGVNPass(pass);
    LLVMAddCFGSimplificationPass(pass);
    LLVMRunPassManager(pass, state.module);
    if (DFG::shouldShowDisassembly() || DFG::verboseCompilationEnabled())
        state.dumpState("after optimization");
    
    // FIXME: LLVM should use our own JIT memory allocator, and we shouldn't have to
    // keep around an LLVMExecutionEngineRef to keep code alive.
    // https://bugs.webkit.org/show_bug.cgi?id=113619
    // FIXME: Need to add support for the case where JIT memory allocation failed.
    // https://bugs.webkit.org/show_bug.cgi?id=113620
    state.generatedFunction = reinterpret_cast<GeneratedFunction>(LLVMGetPointerToGlobal(state.engine, state.function));
    LLVMDisposePassManager(pass);
}

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

