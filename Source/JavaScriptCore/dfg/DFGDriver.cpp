/*
 * Copyright (C) 2011, 2012 Apple Inc. All rights reserved.
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

#include "JSObject.h"
#include "JSString.h"
#include "ScopeChain.h"

#if ENABLE(DFG_JIT)

#include "DFGArgumentsSimplificationPhase.h"
#include "DFGByteCodeParser.h"
#include "DFGCFAPhase.h"
#include "DFGCFGSimplificationPhase.h"
#include "DFGCSEPhase.h"
#include "DFGConstantFoldingPhase.h"
#include "DFGFixupPhase.h"
#include "DFGJITCompiler.h"
#include "DFGPredictionPropagationPhase.h"
#include "DFGRedundantPhiEliminationPhase.h"
#include "DFGStructureCheckHoistingPhase.h"
#include "DFGValidate.h"
#include "DFGVirtualRegisterAllocationPhase.h"
#include "Options.h"

namespace JSC { namespace DFG {

static unsigned numCompilations;

unsigned getNumCompilations()
{
    return numCompilations;
}

enum CompileMode { CompileFunction, CompileOther };
inline bool compile(CompileMode compileMode, ExecState* exec, CodeBlock* codeBlock, JITCode& jitCode, MacroAssemblerCodePtr* jitCodeWithArityCheck)
{
    SamplingRegion samplingRegion("DFG Compilation (Driver)");
    
    numCompilations++;
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->alternative());
    ASSERT(codeBlock->alternative()->getJITType() == JITCode::BaselineJIT);

    if (!Options::useDFGJIT())
        return false;
    
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("DFG compiling code block %p(%p) for executable %p, number of instructions = %u.\n", codeBlock, codeBlock->alternative(), codeBlock->ownerExecutable(), codeBlock->instructionCount());
#endif
    
    Graph dfg(exec->globalData(), codeBlock);
    if (!parse(exec, dfg))
        return false;
    
    if (compileMode == CompileFunction)
        dfg.predictArgumentTypes();
    
    // By this point the DFG bytecode parser will have potentially mutated various tables
    // in the CodeBlock. This is a good time to perform an early shrink, which is more
    // powerful than a late one. It's safe to do so because we haven't generated any code
    // that references any of the tables directly, yet.
    codeBlock->shrinkToFit(CodeBlock::EarlyShrink);

    validate(dfg);
    performPredictionPropagation(dfg);
    performFixup(dfg);
    performStructureCheckHoisting(dfg);
    unsigned cnt = 1;
    for (;; ++cnt) {
#if DFG_ENABLE(DEBUG_VERBOSE)
        dataLog("DFG beginning optimization fixpoint iteration #%u.\n", cnt);
#endif
        bool changed = false;
        performCFA(dfg);
        changed |= performConstantFolding(dfg);
        changed |= performArgumentsSimplification(dfg);
        changed |= performCFGSimplification(dfg);
        changed |= performCSE(dfg, FixpointNotConverged);
        if (!changed)
            break;
        dfg.resetExitStates();
        performFixup(dfg);
    }
    performCSE(dfg, FixpointConverged);
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("DFG optimization fixpoint converged in %u iterations.\n", cnt);
#endif
    performVirtualRegisterAllocation(dfg);

    GraphDumpMode modeForFinalValidate = DumpGraph;
#if DFG_ENABLE(DEBUG_VERBOSE)
    dataLog("Graph after optimization:\n");
    dfg.dump();
    modeForFinalValidate = DontDumpGraph;
#endif
    validate(dfg, modeForFinalValidate);
    
    JITCompiler dataFlowJIT(dfg);
    bool result;
    if (compileMode == CompileFunction) {
        ASSERT(jitCodeWithArityCheck);
        
        result = dataFlowJIT.compileFunction(jitCode, *jitCodeWithArityCheck);
    } else {
        ASSERT(compileMode == CompileOther);
        ASSERT(!jitCodeWithArityCheck);
        
        result = dataFlowJIT.compile(jitCode);
    }
    
    return result;
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

