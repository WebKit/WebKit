/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef JITDriver_h
#define JITDriver_h

#include <wtf/Platform.h>

#if ENABLE(JIT)

#include "BytecodeGenerator.h"
#include "DFGDriver.h"
#include "JIT.h"
#include "LLIntEntrypoints.h"

namespace JSC {

template<typename CodeBlockType>
inline bool jitCompileIfAppropriate(ExecState* exec, CodeBlockType* codeBlock, RefPtr<JITCode>& jitCode, JITCode::JITType jitType, unsigned bytecodeIndex, JITCompilationEffort effort)
{
    VM& vm = exec->vm();
    
    if (jitType == codeBlock->getJITType())
        return true;
    
    if (!vm.canUseJIT())
        return true;
    
    codeBlock->unlinkIncomingCalls();
    
    RefPtr<JITCode> oldJITCode = jitCode;
    
    if (JITCode::isOptimizingJIT(jitType)) {
        if (DFG::tryCompile(exec, codeBlock, jitCode, bytecodeIndex))
            codeBlock->alternative()->unlinkIncomingCalls();
        else {
            ASSERT(oldJITCode == jitCode);
            return false;
        }
    } else {
        RefPtr<JITCode> result = JIT::compile(&vm, codeBlock, effort);
        if (result)
            jitCode = result;
        else
            return false;
    }
    
    codeBlock->setJITCode(jitCode, MacroAssemblerCodePtr());
    
    return true;
}

inline bool jitCompileFunctionIfAppropriate(ExecState* exec, FunctionCodeBlock* codeBlock, RefPtr<JITCode>& jitCode, MacroAssemblerCodePtr& jitCodeWithArityCheck, JITCode::JITType jitType, unsigned bytecodeIndex, JITCompilationEffort effort)
{
    VM& vm = exec->vm();
    
    if (jitType == codeBlock->getJITType())
        return true;
    
    if (!vm.canUseJIT())
        return true;
    
    codeBlock->unlinkIncomingCalls();
    
    RefPtr<JITCode> oldJITCode = jitCode;
    MacroAssemblerCodePtr oldJITCodeWithArityCheck = jitCodeWithArityCheck;
    
    if (JITCode::isOptimizingJIT(jitType)) {
        if (DFG::tryCompileFunction(exec, codeBlock, jitCode, jitCodeWithArityCheck, bytecodeIndex))
            codeBlock->alternative()->unlinkIncomingCalls();
        else {
            ASSERT(oldJITCode == jitCode);
            ASSERT_UNUSED(oldJITCodeWithArityCheck, oldJITCodeWithArityCheck == jitCodeWithArityCheck);
            return false;
        }
    } else {
        RefPtr<JITCode> result =
            JIT::compile(&vm, codeBlock, effort, &jitCodeWithArityCheck);
        if (result)
            jitCode = result;
        else {
            ASSERT_UNUSED(oldJITCodeWithArityCheck, oldJITCodeWithArityCheck == jitCodeWithArityCheck);
            return false;
        }
    }

    codeBlock->setJITCode(jitCode, jitCodeWithArityCheck);
    
    return true;
}

} // namespace JSC

#endif // ENABLE(JIT)

#endif // JITDriver_h

