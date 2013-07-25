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

#ifndef DFGPlan_h
#define DFGPlan_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGDesiredIdentifiers.h"
#include "DFGDesiredStructureChains.h"
#include "DFGDesiredWatchpoints.h"
#include "DFGFinalizer.h"
#include "ProfilerCompilation.h"
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace DFG {

enum CompileMode { CompileFunction, CompileOther };
enum CompilationResult { CompilationFailed, CompilationInvalidated, CompilationSuccessful };

struct Plan : public ThreadSafeRefCounted<Plan> {
    Plan(
        CompileMode compileMode, CodeBlock* codeBlock, unsigned osrEntryBytecodeIndex,
        unsigned numVarsWithValues);
    ~Plan();
    
    void compileInThread();
    
    CompilationResult finalize(RefPtr<JSC::JITCode>& jitCode, MacroAssemblerCodePtr* jitCodeWithArityCheck);
    
    VM& vm() { return *codeBlock->vm(); }

    const CompileMode compileMode;
    CodeBlock* const codeBlock;
    const unsigned osrEntryBytecodeIndex;
    const unsigned numVarsWithValues;
    Operands<JSValue> mustHandleValues;

    RefPtr<Profiler::Compilation> compilation;

    OwnPtr<Finalizer> finalizer;
    
    DesiredWatchpoints watchpoints;
    DesiredIdentifiers identifiers;
    DesiredStructureChains chains;

private:
    bool isStillValid();
    void reallyAdd();
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGPlan_h

