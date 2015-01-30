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
#include "ExitingJITType.h"
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
        : m_couldTakeSlowPath(false)
        , m_isProved(false)
        , m_canTrustCounts(false)
    {
    }
    
    static CallLinkStatus takesSlowPath()
    {
        CallLinkStatus result;
        result.m_couldTakeSlowPath = true;
        return result;
    }
    
    explicit CallLinkStatus(JSValue);
    
    CallLinkStatus(CallVariant variant)
        : m_edges(1, CallEdge(variant, 1))
        , m_couldTakeSlowPath(false)
        , m_isProved(false)
        , m_canTrustCounts(false)
    {
    }
    
    CallLinkStatus& setIsProved(bool isProved)
    {
        m_isProved = isProved;
        return *this;
    }
    
    static CallLinkStatus computeFor(
        CodeBlock*, unsigned bytecodeIndex, const CallLinkInfoMap&);

    struct ExitSiteData {
        ExitSiteData()
            : m_takesSlowPath(false)
            , m_badFunction(false)
        {
        }
        
        bool m_takesSlowPath;
        bool m_badFunction;
    };
    static ExitSiteData computeExitSiteData(const ConcurrentJITLocker&, CodeBlock*, unsigned bytecodeIndex, ExitingJITType = ExitFromAnything);
    
#if ENABLE(JIT)
    // Computes the status assuming that we never took slow path and never previously
    // exited.
    static CallLinkStatus computeFor(const ConcurrentJITLocker&, CodeBlock*, CallLinkInfo&);
    static CallLinkStatus computeFor(
        const ConcurrentJITLocker&, CodeBlock*, CallLinkInfo&, ExitSiteData);
#endif
    
    typedef HashMap<CodeOrigin, CallLinkStatus, CodeOriginApproximateHash> ContextMap;
    
    // Computes all of the statuses of the DFG code block. Doesn't include statuses that had
    // no information. Currently we use this when compiling FTL code, to enable polyvariant
    // inlining.
    static void computeDFGStatuses(CodeBlock* dfgCodeBlock, ContextMap&);
    
    // Helper that first consults the ContextMap and then does computeFor().
    static CallLinkStatus computeFor(
        CodeBlock*, CodeOrigin, const CallLinkInfoMap&, const ContextMap&);
    
    bool isSet() const { return !m_edges.isEmpty() || m_couldTakeSlowPath; }
    
    bool operator!() const { return !isSet(); }
    
    bool couldTakeSlowPath() const { return m_couldTakeSlowPath; }
    
    CallEdgeList edges() const { return m_edges; }
    unsigned size() const { return m_edges.size(); }
    CallEdge at(unsigned i) const { return m_edges[i]; }
    CallEdge operator[](unsigned i) const { return at(i); }
    bool isProved() const { return m_isProved; }
    bool canOptimize() const { return !m_edges.isEmpty(); }
    bool canTrustCounts() const { return m_canTrustCounts; }
    
    bool isClosureCall() const; // Returns true if any callee is a closure call.
    
    void dump(PrintStream&) const;
    
private:
    void makeClosureCall();
    
    static CallLinkStatus computeFromLLInt(const ConcurrentJITLocker&, CodeBlock*, unsigned bytecodeIndex);
#if ENABLE(JIT)
    static CallLinkStatus computeFromCallEdgeProfile(CallEdgeProfile*);
    static CallLinkStatus computeFromCallLinkInfo(
        const ConcurrentJITLocker&, CallLinkInfo&);
#endif
    
    CallEdgeList m_edges;
    bool m_couldTakeSlowPath;
    bool m_isProved;
    bool m_canTrustCounts;
};

} // namespace JSC

#endif // CallLinkStatus_h

