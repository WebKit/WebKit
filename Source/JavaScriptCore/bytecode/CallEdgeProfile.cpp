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

#include "config.h"
#include "CallEdgeProfile.h"

#include "CCallHelpers.h"
#include "CallEdgeProfileInlines.h"
#include "JITOperations.h"
#include "JSCInlines.h"

namespace JSC {

CallEdgeList CallEdgeProfile::callEdges() const
{
    ConcurrentJITLocker locker(m_lock);
    
    CallEdgeList result;
    
    CallVariant primaryCallee = m_primaryCallee;
    CallEdgeCountType numCallsToPrimary = m_numCallsToPrimary;
    // Defend against races. These fields are modified by the log processor without locking.
    if (!!primaryCallee && numCallsToPrimary)
        result.append(CallEdge(primaryCallee, numCallsToPrimary));
    
    if (m_otherCallees) {
        // Make sure that if the primary thread had just created a m_otherCalles while log
        // processing, we see a consistently created one. The lock being held is insufficient for
        // this, since the log processor will only grab the lock when merging the secondary
        // spectrum into the primary one but may still create the data structure without holding
        // locks.
        WTF::loadLoadFence();
        for (CallEdge& entry : m_otherCallees->m_processed) {
            // Defend against the possibility that the primary duplicates an entry in the secondary
            // spectrum. That can happen when the GC removes the primary. We could have the GC fix
            // the situation by changing the primary to be something from the secondary spectrum,
            // but this fix seems simpler to implement and also cheaper.
            if (entry.callee() == result[0].callee()) {
                result[0] = CallEdge(result[0].callee(), entry.count() + result[0].count());
                continue;
            }
            
            result.append(entry);
        }
    }
    
    std::sort(result.begin(), result.end(), [] (const CallEdge& a, const CallEdge& b) -> bool {
            return a.count() > b.count();
        });
    
    if (result.size() >= 2)
        ASSERT(result[0].count() >= result.last().count());
    
    return result;
}

CallEdgeCountType CallEdgeProfile::numCallsToKnownCells() const
{
    CallEdgeCountType result = 0;
    for (CallEdge& edge : callEdges())
        result += edge.count();
    return result;
}

static bool worthDespecifying(const CallVariant& variant)
{
    return !Heap::isMarked(variant.rawCalleeCell())
        && Heap::isMarked(variant.despecifiedClosure().rawCalleeCell());
}

bool CallEdgeProfile::worthDespecifying()
{
    if (m_closuresAreDespecified)
        return false;
    
    bool didSeeEntry = false;
    
    if (!!m_primaryCallee) {
        didSeeEntry = true;
        if (!JSC::worthDespecifying(m_primaryCallee))
            return false;
    }
    
    if (m_otherCallees) {
        for (unsigned i = m_otherCallees->m_processed.size(); i--;) {
            didSeeEntry = true;
            if (!JSC::worthDespecifying(m_otherCallees->m_processed[i].callee()))
                return false;
        }
    }
    
    return didSeeEntry;
}

void CallEdgeProfile::visitWeak()
{
    if (!m_primaryCallee && !m_otherCallees)
        return;
    
    ConcurrentJITLocker locker(m_lock);
    
    // See if anything is dead and if that can be rectified by despecifying.
    if (worthDespecifying()) {
        CallSpectrum newSpectrum;
        
        if (!!m_primaryCallee)
            newSpectrum.add(m_primaryCallee.despecifiedClosure(), m_numCallsToPrimary);
        
        if (m_otherCallees) {
            for (unsigned i = m_otherCallees->m_processed.size(); i--;) {
                newSpectrum.add(
                    m_otherCallees->m_processed[i].callee().despecifiedClosure(),
                    m_otherCallees->m_processed[i].count());
            }
        }
        
        Vector<CallSpectrum::KeyAndCount> list = newSpectrum.buildList();
        RELEASE_ASSERT(list.size());
        m_primaryCallee = list.last().key;
        m_numCallsToPrimary = list.last().count;
        
        if (m_otherCallees) {
            m_otherCallees->m_processed.clear();

            // We could have a situation where the GC clears the primary and then log processing
            // reinstates it without ever doing an addSlow and subsequent mergeBack. In such a case
            // the primary could duplicate an entry in otherCallees, which means that even though we
            // had an otherCallees object, the list size is just 1.
            if (list.size() >= 2) {
                for (unsigned i = list.size() - 1; i--;)
                    m_otherCallees->m_processed.append(CallEdge(list[i].key, list[i].count));
            }
        }
        
        m_closuresAreDespecified = true;
        
        return;
    }
    
    if (!!m_primaryCallee && !Heap::isMarked(m_primaryCallee.rawCalleeCell())) {
        m_numCallsToUnknownCell += m_numCallsToPrimary;
        
        m_primaryCallee = CallVariant();
        m_numCallsToPrimary = 0;
    }
    
    if (m_otherCallees) {
        for (unsigned i = 0; i < m_otherCallees->m_processed.size(); i++) {
            if (Heap::isMarked(m_otherCallees->m_processed[i].callee().rawCalleeCell()))
                continue;
            
            m_numCallsToUnknownCell += m_otherCallees->m_processed[i].count();
            m_otherCallees->m_processed[i--] = m_otherCallees->m_processed.last();
            m_otherCallees->m_processed.removeLast();
        }
        
        // Only exists while we are processing the log.
        RELEASE_ASSERT(!m_otherCallees->m_temporarySpectrum);
    }
}

void CallEdgeProfile::addSlow(CallVariant callee, CallEdgeProfileVector& mergeBackLog)
{
    // This exists to handle cases where the spectrum wasn't created yet, or we're storing to a
    // particular spectrum for the first time during a log processing iteration.
    
    if (!m_otherCallees) {
        m_otherCallees = std::make_unique<Secondary>();
        // If a compiler thread notices the m_otherCallees being non-null, we want to make sure
        // that it sees a fully created one.
        WTF::storeStoreFence();
    }
    
    if (!m_otherCallees->m_temporarySpectrum) {
        m_otherCallees->m_temporarySpectrum = std::make_unique<CallSpectrum>();
        for (unsigned i = m_otherCallees->m_processed.size(); i--;) {
            m_otherCallees->m_temporarySpectrum->add(
                m_otherCallees->m_processed[i].callee(),
                m_otherCallees->m_processed[i].count());
        }
        
        // This means that this is the first time we're seeing this profile during this log
        // processing iteration.
        mergeBackLog.append(this);
    }
    
    m_otherCallees->m_temporarySpectrum->add(callee);
}

void CallEdgeProfile::mergeBack()
{
    ConcurrentJITLocker locker(m_lock);
    
    RELEASE_ASSERT(m_otherCallees);
    RELEASE_ASSERT(m_otherCallees->m_temporarySpectrum);
    
    if (!!m_primaryCallee)
        m_otherCallees->m_temporarySpectrum->add(m_primaryCallee, m_numCallsToPrimary);
    
    if (!m_closuresAreDespecified) {
        CallSpectrum newSpectrum;
        for (auto& entry : *m_otherCallees->m_temporarySpectrum)
            newSpectrum.add(entry.key.despecifiedClosure(), entry.value);
        
        if (newSpectrum.size() < m_otherCallees->m_temporarySpectrum->size()) {
            *m_otherCallees->m_temporarySpectrum = newSpectrum;
            m_closuresAreDespecified = true;
        }
    }
    
    Vector<CallSpectrum::KeyAndCount> list = m_otherCallees->m_temporarySpectrum->buildList();
    m_otherCallees->m_temporarySpectrum = nullptr;
    
    m_primaryCallee = list.last().key;
    m_numCallsToPrimary = list.last().count;
    list.removeLast();
    
    m_otherCallees->m_processed.clear();
    for (unsigned count = maxKnownCallees; count-- && !list.isEmpty();) {
        m_otherCallees->m_processed.append(CallEdge(list.last().key, list.last().count));
        list.removeLast();
    }
    
    for (unsigned i = list.size(); i--;)
        m_numCallsToUnknownCell += list[i].count;
}

void CallEdgeProfile::fadeByHalf()
{
    m_numCallsToPrimary >>= 1;
    m_numCallsToNotCell >>= 1;
    m_numCallsToUnknownCell >>= 1;
    m_totalCount >>= 1;
    
    if (m_otherCallees) {
        for (unsigned i = m_otherCallees->m_processed.size(); i--;) {
            m_otherCallees->m_processed[i] = CallEdge(
                m_otherCallees->m_processed[i].callee(),
                m_otherCallees->m_processed[i].count() >> 1);
        }
        
        if (m_otherCallees->m_temporarySpectrum) {
            for (auto& entry : *m_otherCallees->m_temporarySpectrum)
                entry.value >>= 1;
        }
    }
}

CallEdgeLog::CallEdgeLog()
    : m_scaledLogIndex(logSize * sizeof(Entry))
{
    ASSERT(!(m_scaledLogIndex % sizeof(Entry)));
}

CallEdgeLog::~CallEdgeLog() { }

bool CallEdgeLog::isEnabled()
{
    return Options::enableCallEdgeProfiling() && Options::useFTLJIT();
}

#if ENABLE(JIT)

extern "C" JIT_OPERATION void operationProcessCallEdgeLog(CallEdgeLog*) WTF_INTERNAL;
extern "C" JIT_OPERATION void operationProcessCallEdgeLog(CallEdgeLog* log)
{
    log->processLog();
}

void CallEdgeLog::emitLogCode(CCallHelpers& jit, CallEdgeProfile& profile, JSValueRegs calleeRegs)
{
    const unsigned numberOfArguments = 1;
    
    GPRReg scratchGPR;
    if (!calleeRegs.uses(GPRInfo::regT0))
        scratchGPR = GPRInfo::regT0;
    else if (!calleeRegs.uses(GPRInfo::regT1))
        scratchGPR = GPRInfo::regT1;
    else
        scratchGPR = GPRInfo::regT2;
    
    jit.load32(&m_scaledLogIndex, scratchGPR);
    
    CCallHelpers::Jump ok = jit.branchTest32(CCallHelpers::NonZero, scratchGPR);
    
    ASSERT_UNUSED(numberOfArguments, stackAlignmentRegisters() >= 1 + numberOfArguments);
    
    jit.subPtr(CCallHelpers::TrustedImm32(stackAlignmentBytes()), CCallHelpers::stackPointerRegister);
    
    jit.storeValue(calleeRegs, CCallHelpers::Address(CCallHelpers::stackPointerRegister, sizeof(JSValue)));
    jit.setupArguments(CCallHelpers::TrustedImmPtr(this));
    jit.move(CCallHelpers::TrustedImmPtr(bitwise_cast<void*>(operationProcessCallEdgeLog)), GPRInfo::nonArgGPR0);
    jit.call(GPRInfo::nonArgGPR0);
    jit.loadValue(CCallHelpers::Address(CCallHelpers::stackPointerRegister, sizeof(JSValue)), calleeRegs);
    
    jit.addPtr(CCallHelpers::TrustedImm32(stackAlignmentBytes()), CCallHelpers::stackPointerRegister);
    
    jit.move(CCallHelpers::TrustedImm32(logSize * sizeof(Entry)), scratchGPR);
    ok.link(&jit);

    jit.sub32(CCallHelpers::TrustedImm32(sizeof(Entry)), scratchGPR);
    jit.store32(scratchGPR, &m_scaledLogIndex);
    jit.addPtr(CCallHelpers::TrustedImmPtr(m_log), scratchGPR);
    jit.storeValue(calleeRegs, CCallHelpers::Address(scratchGPR, OBJECT_OFFSETOF(Entry, m_value)));
    jit.storePtr(CCallHelpers::TrustedImmPtr(&profile), CCallHelpers::Address(scratchGPR, OBJECT_OFFSETOF(Entry, m_profile)));
}

void CallEdgeLog::emitLogCode(
    CCallHelpers& jit, OwnPtr<CallEdgeProfile>& profilePointer, JSValueRegs calleeRegs)
{
    if (!isEnabled())
        return;
    
    profilePointer.createTransactionally();
    emitLogCode(jit, *profilePointer, calleeRegs);
}

#endif // ENABLE(JIT)

void CallEdgeLog::processLog()
{
    ASSERT(!(m_scaledLogIndex % sizeof(Entry)));
    
    if (Options::callEdgeProfileReallyProcessesLog()) {
        CallEdgeProfileVector mergeBackLog;
        
        for (unsigned i = m_scaledLogIndex / sizeof(Entry); i < logSize; ++i)
            m_log[i].m_profile->add(m_log[i].m_value, mergeBackLog);
        
        for (unsigned i = mergeBackLog.size(); i--;)
            mergeBackLog[i]->mergeBack();
    }
    
    m_scaledLogIndex = logSize * sizeof(Entry);
}

} // namespace JSC

