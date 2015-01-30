/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CallEdgeProfile_h
#define CallEdgeProfile_h

#include "CallEdge.h"
#include "CallVariant.h"
#include "ConcurrentJITLock.h"
#include "GPRInfo.h"
#include "JSCell.h"
#include <wtf/OwnPtr.h>

namespace JSC {

class CCallHelpers;
class LLIntOffsetsExtractor;

class CallEdgeLog;
class CallEdgeProfile;
typedef Vector<CallEdgeProfile*, 10> CallEdgeProfileVector;

class CallEdgeProfile {
public:
    CallEdgeProfile();
    
    CallEdgeCountType numCallsToNotCell() const { return m_numCallsToNotCell; }
    CallEdgeCountType numCallsToUnknownCell() const { return m_numCallsToUnknownCell; }
    CallEdgeCountType numCallsToKnownCells() const;
    
    CallEdgeCountType totalCalls() const { return m_totalCount; }
    
    // Call while holding the owning CodeBlock's lock.
    CallEdgeList callEdges() const;
    
    void visitWeak();
    
private:
    friend class CallEdgeLog;
    
    static const unsigned maxKnownCallees = 5;
    
    void add(JSValue, CallEdgeProfileVector& mergeBackLog);
    
    bool worthDespecifying();
    void addSlow(CallVariant, CallEdgeProfileVector& mergeBackLog);
    void mergeBack();
    void fadeByHalf();
    
    // It's cheaper to let this have its own lock. It needs to be able to find which lock to
    // lock. Normally it would lock the owning CodeBlock's lock, but that would require a
    // pointer-width word to point at the CodeBlock. Having the byte-sized lock here is
    // cheaper. However, this means that the relationship with the CodeBlock lock is:
    // acquire the CodeBlock lock before this one.
    mutable ConcurrentJITLock m_lock;
    
    bool m_closuresAreDespecified;
    
    CallEdgeCountType m_numCallsToPrimary;
    CallEdgeCountType m_numCallsToNotCell;
    CallEdgeCountType m_numCallsToUnknownCell;
    CallEdgeCountType m_totalCount;
    CallVariant m_primaryCallee;
    
    typedef Spectrum<CallVariant, CallEdgeCountType> CallSpectrum;
    
    struct Secondary {
        Vector<CallEdge> m_processed; // Processed but not necessarily sorted.
        std::unique_ptr<CallSpectrum> m_temporarySpectrum;
    };
    
    std::unique_ptr<Secondary> m_otherCallees;
};

class CallEdgeLog {
public:
    CallEdgeLog();
    ~CallEdgeLog();

    static bool isEnabled();
    
#if ENABLE(JIT)
    void emitLogCode(CCallHelpers&, CallEdgeProfile&, JSValueRegs calleeRegs); // Assumes that stack is aligned, all volatile registers - other than calleeGPR - are clobberable, and the parameter space is in use.
    
    // Same as above but creates a CallEdgeProfile instance if one did not already exist. Does
    // this in a thread-safe manner by calling OwnPtr::createTransactionally.
    void emitLogCode(CCallHelpers&, OwnPtr<CallEdgeProfile>&, JSValueRegs calleeRegs);
#endif // ENABLE(JIT)
    
    void processLog();
    
private:
    friend class LLIntOffsetsExtractor;

    static const unsigned logSize = 10000;
    
    struct Entry {
        JSValue m_value;
        CallEdgeProfile* m_profile;
    };
    
    unsigned m_scaledLogIndex;
    Entry m_log[logSize];
};

} // namespace JSC

#endif // CallEdgeProfile_h

