/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "DFGDriver.h"

#if ENABLE(DFG_JIT)

#include "DFGByteCodeParser.h"
#include "DFGJITCompiler.h"
#include "DFGPropagator.h"

namespace JSC { namespace DFG {

enum CompileMode { CompileFunction, CompileOther };
inline bool compile(CompileMode compileMode, ExecState* exec, CodeBlock* codeBlock, JITCode& jitCode, MacroAssemblerCodePtr* jitCodeWithArityCheck)
{
    SamplingRegion samplingRegion("DFG Compilation (Driver)");
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->alternative());
    ASSERT(codeBlock->alternative()->getJITType() == JITCode::BaselineJIT);

#if DFG_ENABLE(DEBUG_VERBOSE)
    fprintf(stderr, "DFG compiling code block %p(%p), number of instructions = %u.\n", codeBlock, codeBlock->alternative(), codeBlock->instructionCount());
#endif
    
    JSGlobalData* globalData = &exec->globalData();
    Graph dfg;
    if (!parse(dfg, globalData, codeBlock))
        return false;
    
    if (compileMode == CompileFunction)
        dfg.predictArgumentTypes(codeBlock);
    
    propagate(dfg, globalData, codeBlock);
    
    JITCompiler dataFlowJIT(globalData, dfg, codeBlock);
    if (compileMode == CompileFunction) {
        ASSERT(jitCodeWithArityCheck);
        
        dataFlowJIT.compileFunction(jitCode, *jitCodeWithArityCheck);
    } else {
        ASSERT(compileMode == CompileOther);
        ASSERT(!jitCodeWithArityCheck);
        
        dataFlowJIT.compile(jitCode);
    }
    
    return true;
}

bool tryCompile(ExecState* exec, CodeBlock* codeBlock, JITCode& jitCode)
{
    return compile(CompileOther, exec, codeBlock, jitCode, 0);
}

bool tryCompileFunction(ExecState* exec, CodeBlock* codeBlock, JITCode& jitCode, MacroAssemblerCodePtr& jitCodeWithArityCheck)
{
    return compile(CompileFunction, exec, codeBlock, jitCode, &jitCodeWithArityCheck);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

