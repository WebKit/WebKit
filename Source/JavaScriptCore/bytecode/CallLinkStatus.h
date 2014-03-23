/*
 * Copyright (C) 2012, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef CallLinkStatus_h
#define CallLinkStatus_h

#include "CallLinkInfo.h"
#include "CodeOrigin.h"
#include "CodeSpecializationKind.h"
#include "ConcurrentJITLock.h"
#include "Intrinsic.h"
#include "JSCJSValue.h"

namespace JSC {

class CodeBlock;
class ExecutableBase;
class InternalFunction;
class JSFunction;
class Structure;
struct CallLinkInfo;

class CallLinkStatus {
public:
    CallLinkStatus()
        : m_executable(0)
        , m_structure(0)
        , m_couldTakeSlowPath(false)
        , m_isProved(false)
    {
    }
    
    static CallLinkStatus takesSlowPath()
    {
        CallLinkStatus result;
        result.m_couldTakeSlowPath = true;
        return result;
    }
    
    explicit CallLinkStatus(JSValue);
    
    CallLinkStatus(ExecutableBase* executable, Structure* structure)
        : m_executable(executable)
        , m_structure(structure)
        , m_couldTakeSlowPath(false)
        , m_isProved(false)
    {
        ASSERT(!!executable == !!structure);
    }
    
    CallLinkStatus& setIsProved(bool isProved)
    {
        m_isProved = isProved;
        return *this;
    }
    
    static CallLinkStatus computeFor(
        CodeBlock*, unsigned bytecodeIndex, const CallLinkInfoMap&);

#if ENABLE(JIT)
    // Computes the status assuming that we never took slow path and never previously
    // exited.
    static CallLinkStatus computeFor(const ConcurrentJITLocker&, CallLinkInfo&);
#endif
    
    typedef HashMap<CodeOrigin, CallLinkStatus, CodeOriginApproximateHash> ContextMap;
    
    // Computes all of the statuses of the DFG code block. Doesn't include statuses that had
    // no information. Currently we use this when compiling FTL code, to enable polyvariant
    // inlining.
    static void computeDFGStatuses(CodeBlock* dfgCodeBlock, ContextMap&);
    
    // Helper that first consults the ContextMap and then does computeFor().
    static CallLinkStatus computeFor(
        CodeBlock*, CodeOrigin, const CallLinkInfoMap&, const ContextMap&);
    
    bool isSet() const { return m_callTarget || m_executable || m_couldTakeSlowPath; }
    
    bool operator!() const { return !isSet(); }
    
    bool couldTakeSlowPath() const { return m_couldTakeSlowPath; }
    bool isClosureCall() const { return m_executable && !m_callTarget; }
    
    JSValue callTarget() const { return m_callTarget; }
    JSFunction* function() const;
    InternalFunction* internalFunction() const;
    Intrinsic intrinsicFor(CodeSpecializationKind) const;
    ExecutableBase* executable() const { return m_executable; }
    Structure* structure() const { return m_structure; }
    bool isProved() const { return m_isProved; }
    bool canOptimize() const { return (m_callTarget || m_executable) && !m_couldTakeSlowPath; }
    
    void dump(PrintStream&) const;
    
private:
    void makeClosureCall()
    {
        ASSERT(!m_isProved);
        // Turn this into a closure call.
        m_callTarget = JSValue();
    }
    
    static CallLinkStatus computeFromLLInt(const ConcurrentJITLocker&, CodeBlock*, unsigned bytecodeIndex);
    
    JSValue m_callTarget;
    ExecutableBase* m_executable;
    Structure* m_structure;
    bool m_couldTakeSlowPath;
    bool m_isProved;
};

} // namespace JSC

#endif // CallLinkStatus_h

