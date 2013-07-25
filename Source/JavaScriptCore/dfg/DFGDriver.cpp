/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGJITCode.h"
#include "DFGPlan.h"
#include "DFGThunks.h"
#include "DFGWorklist.h"
#include "FTLThunks.h"
#include "JITCode.h"
#include "Operations.h"
#include "Options.h"
#include "SamplingTool.h"
#include <wtf/Atomics.h>

namespace JSC { namespace DFG {

static unsigned numCompilations;

unsigned getNumCompilations()
{
    return numCompilations;
}

static CompilationResult compile(CompileMode compileMode, ExecState* exec, CodeBlock* codeBlock, RefPtr<JSC::JITCode>& jitCode, MacroAssemblerCodePtr* jitCodeWithArityCheck, unsigned osrEntryBytecodeIndex)
{
    SamplingRegion samplingRegion("DFG Compilation (Driver)");
    
    numCompilations++;
    
    ASSERT(codeBlock);
    ASSERT(codeBlock->alternative());
    ASSERT(codeBlock->alternative()->jitType() == JITCode::BaselineJIT);
    
    ASSERT(osrEntryBytecodeIndex != UINT_MAX);

    if (!Options::useDFGJIT() || !MacroAssembler::supportsFloatingPoint())
        return CompilationFailed;

    if (!Options::bytecodeRangeToDFGCompile().isInRange(codeBlock->instructionCount()))
        return CompilationFailed;

    if (logCompilationChanges())
        dataLog("DFG(Driver) compiling ", *codeBlock, ", number of instructions = ", codeBlock->instructionCount(), "\n");
    
    VM& vm = exec->vm();
    
    // Make sure that any stubs that the DFG is going to use are initialized. We want to
    // make sure that al JIT code generation does finalization on the main thread.
    vm.getCTIStub(osrExitGenerationThunkGenerator);
    vm.getCTIStub(throwExceptionFromCallSlowPathGenerator);
    vm.getCTIStub(linkCallThunkGenerator);
    vm.getCTIStub(linkConstructThunkGenerator);
    vm.getCTIStub(linkClosureCallThunkGenerator);
    vm.getCTIStub(virtualCallThunkGenerator);
    vm.getCTIStub(virtualConstructThunkGenerator);
#if ENABLE(FTL_JIT)
    vm.getCTIStub(FTL::osrExitGenerationThunkGenerator);
#endif
    
    // Derive our set of must-handle values. The compilation must be at least conservative
    // enough to allow for OSR entry with these values.
    unsigned numVarsWithValues;
    if (osrEntryBytecodeIndex)
        numVarsWithValues = codeBlock->m_numVars;
    else
        numVarsWithValues = 0;
    RefPtr<Plan> plan = adoptRef(
        new Plan(compileMode, codeBlock, osrEntryBytecodeIndex, numVarsWithValues));
    for (size_t i = 0; i < plan->mustHandleValues.size(); ++i) {
        int operand = plan->mustHandleValues.operandForIndex(i);
        if (operandIsArgument(operand)
            && !operandToArgument(operand)
            && compileMode == CompileFunction
            && codeBlock->specializationKind() == CodeForConstruct) {
            // Ugh. If we're in a constructor, the 'this' argument may hold garbage. It will
            // also never be used. It doesn't matter what we put into the value for this,
            // but it has to be an actual value that can be grokked by subsequent DFG passes,
            // so we sanitize it here by turning it into Undefined.
            plan->mustHandleValues[i] = jsUndefined();
        } else
            plan->mustHandleValues[i] = exec->uncheckedR(operand).jsValue();
    }
    
    if (enableConcurrentJIT()) {
        if (!vm.worklist)
            vm.worklist = globalWorklist();
        if (logCompilationChanges())
            dataLog("Deferring DFG compilation of ", *codeBlock, " with queue length ", vm.worklist->queueLength(), ".\n");
        vm.worklist->enqueue(plan);
        return CompilationDeferred;
    }
    
    plan->compileInThread(*vm.dfgState);
    return plan->finalize(jitCode, jitCodeWithArityCheck);
}

CompilationResult tryCompile(ExecState* exec, CodeBlock* codeBlock, RefPtr<JSC::JITCode>& jitCode, unsigned bytecodeIndex)
{
    return compile(CompileOther, exec, codeBlock, jitCode, 0, bytecodeIndex);
}

CompilationResult tryCompileFunction(ExecState* exec, CodeBlock* codeBlock, RefPtr<JSC::JITCode>& jitCode, MacroAssemblerCodePtr& jitCodeWithArityCheck, unsigned bytecodeIndex)
{
    return compile(CompileFunction, exec, codeBlock, jitCode, &jitCodeWithArityCheck, bytecodeIndex);
}

CompilationResult tryFinalizePlan(PassRefPtr<Plan> plan, RefPtr<JSC::JITCode>& jitCode, MacroAssemblerCodePtr* jitCodeWithArityCheck)
{
    return plan->finalize(jitCode, jitCodeWithArityCheck);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

