/*
 * Copyright (C) 2011-2015 Apple Inc. All rights reserved.
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

#pragma once

#include "CodeBlock.h"
#include "DFGCommon.h"
#include "Options.h"

namespace JSC { namespace DFG {

#if ENABLE(DFG_JIT)
// Fast check functions; if they return true it is still necessary to
// check opcodes.
bool isSupported();
bool isSupportedForInlining(CodeBlock*);
bool mightCompileEval(CodeBlock*);
bool mightCompileProgram(CodeBlock*);
bool mightCompileFunctionForCall(CodeBlock*);
bool mightCompileFunctionForConstruct(CodeBlock*);
bool mightInlineFunctionForCall(JITType, CodeBlock*);
bool mightInlineFunctionForClosureCall(JITType, CodeBlock*);
bool mightInlineFunctionForConstruct(JITType, CodeBlock*);
bool canUseOSRExitFuzzing(CodeBlock*);
#else // ENABLE(DFG_JIT)
inline bool mightCompileEval(CodeBlock*) { return false; }
inline bool mightCompileProgram(CodeBlock*) { return false; }
inline bool mightCompileFunctionForCall(CodeBlock*) { return false; }
inline bool mightCompileFunctionForConstruct(CodeBlock*) { return false; }
inline bool mightInlineFunctionForCall(JITType, CodeBlock*) { return false; }
inline bool mightInlineFunctionForClosureCall(JITType, CodeBlock*) { return false; }
inline bool mightInlineFunctionForConstruct(JITType, CodeBlock*) { return false; }
inline bool canUseOSRExitFuzzing(CodeBlock*) { return false; }
#endif // ENABLE(DFG_JIT)

inline CapabilityLevel evalCapabilityLevel(CodeBlock* codeBlock)
{
    if (!mightCompileEval(codeBlock))
        return CannotCompile;
    
    return CanCompileAndInline;
}

inline CapabilityLevel programCapabilityLevel(CodeBlock* codeBlock)
{
    if (!mightCompileProgram(codeBlock))
        return CannotCompile;
    
    return CanCompileAndInline;
}

inline CapabilityLevel functionCapabilityLevel(bool mightCompile, bool mightInline, CapabilityLevel computedCapabilityLevel)
{
    if (mightCompile && mightInline)
        return leastUpperBound(CanCompileAndInline, computedCapabilityLevel);
    if (mightCompile && !mightInline)
        return leastUpperBound(CanCompile, computedCapabilityLevel);
    if (!mightCompile)
        return CannotCompile;
    RELEASE_ASSERT_NOT_REACHED();
    return CannotCompile;
}

inline CapabilityLevel functionForCallCapabilityLevel(JITType jitType, CodeBlock* codeBlock)
{
    return functionCapabilityLevel(
        mightCompileFunctionForCall(codeBlock),
        mightInlineFunctionForCall(jitType, codeBlock),
        CanCompileAndInline);
}

inline CapabilityLevel functionForConstructCapabilityLevel(JITType jitType, CodeBlock* codeBlock)
{
    return functionCapabilityLevel(
        mightCompileFunctionForConstruct(codeBlock),
        mightInlineFunctionForConstruct(jitType, codeBlock),
        CanCompileAndInline);
}

inline CapabilityLevel inlineFunctionForCallCapabilityLevel(JITType jitType, CodeBlock* codeBlock)
{
    if (!mightInlineFunctionForCall(jitType, codeBlock))
        return CannotCompile;
    
    return CanCompileAndInline;
}

inline CapabilityLevel inlineFunctionForClosureCallCapabilityLevel(JITType jitType, CodeBlock* codeBlock)
{
    if (!mightInlineFunctionForClosureCall(jitType, codeBlock))
        return CannotCompile;
    
    return CanCompileAndInline;
}

inline CapabilityLevel inlineFunctionForConstructCapabilityLevel(JITType jitType, CodeBlock* codeBlock)
{
    if (!mightInlineFunctionForConstruct(jitType, codeBlock))
        return CannotCompile;
    
    return CanCompileAndInline;
}

inline bool mightInlineFunctionFor(JITType jitType, CodeBlock* codeBlock, CodeSpecializationKind kind)
{
    if (kind == CodeForCall)
        return mightInlineFunctionForCall(jitType, codeBlock);
    ASSERT(kind == CodeForConstruct);
    return mightInlineFunctionForConstruct(jitType, codeBlock);
}

inline bool mightCompileFunctionFor(CodeBlock* codeBlock, CodeSpecializationKind kind)
{
    if (kind == CodeForCall)
        return mightCompileFunctionForCall(codeBlock);
    ASSERT(kind == CodeForConstruct);
    return mightCompileFunctionForConstruct(codeBlock);
}

inline bool mightInlineFunction(JITType jitType, CodeBlock* codeBlock)
{
    return mightInlineFunctionFor(jitType, codeBlock, codeBlock->specializationKind());
}

inline CapabilityLevel inlineFunctionForCapabilityLevel(JITType jitType, CodeBlock* codeBlock, CodeSpecializationKind kind, bool isClosureCall)
{
    if (isClosureCall) {
        if (kind != CodeForCall)
            return CannotCompile;
        return inlineFunctionForClosureCallCapabilityLevel(jitType, codeBlock);
    }
    if (kind == CodeForCall)
        return inlineFunctionForCallCapabilityLevel(jitType, codeBlock);
    ASSERT(kind == CodeForConstruct);
    return inlineFunctionForConstructCapabilityLevel(jitType, codeBlock);
}

inline bool isSmallEnoughToInlineCodeInto(CodeBlock* codeBlock)
{
    return codeBlock->bytecodeCost() <= Options::maximumInliningCallerBytecodeCost();
}

} } // namespace JSC::DFG
