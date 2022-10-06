/*
 * Copyright (C) 2014-2021 Apple Inc. All rights reserved.
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
#include "JITSafepoint.h"

#if ENABLE(JIT)

#include "JITPlan.h"
#include "JITScannable.h"
#include "JITWorklistThread.h"
#include "SlotVisitor.h"

namespace JSC {

Safepoint::Result::~Result()
{
    RELEASE_ASSERT(m_wasChecked);
}

bool Safepoint::Result::didGetCancelled()
{
    m_wasChecked = true;
    return m_didGetCancelled;
}

Safepoint::Safepoint(JITPlan& plan, Result& result)
    : m_vm(plan.vm())
    , m_plan(plan)
    , m_didCallBegin(false)
    , m_result(result)
{
    RELEASE_ASSERT(result.m_wasChecked);
    result.m_wasChecked = false;
    result.m_didGetCancelled = false;
}

Safepoint::~Safepoint()
{
    RELEASE_ASSERT(m_didCallBegin);
    if (JITWorklistThread* thread = m_plan.thread()) {
        RELEASE_ASSERT(thread->m_safepoint == this);
        thread->m_rightToRun.lock();
        thread->m_safepoint = nullptr;
    }
}

void Safepoint::add(Scannable* scannable)
{
    RELEASE_ASSERT(!m_didCallBegin);
    m_scannables.append(scannable);
}

void Safepoint::begin() WTF_IGNORES_THREAD_SAFETY_ANALYSIS
{
    RELEASE_ASSERT(!m_didCallBegin);
    m_didCallBegin = true;
    if (JITWorklistThread* data = m_plan.thread()) {
        RELEASE_ASSERT(!data->m_safepoint);
        data->m_safepoint = this;
        data->m_rightToRun.unlockFairly();
    }
}

template<typename Visitor>
void Safepoint::checkLivenessAndVisitChildren(Visitor& visitor)
{
    RELEASE_ASSERT(m_didCallBegin);

    if (m_result.m_didGetCancelled)
        return; // We were cancelled during a previous GC!
    
    if (!isKnownToBeLiveDuringGC(visitor))
        return;
    
    for (unsigned i = m_scannables.size(); i--;)
        m_scannables[i]->visitChildren(visitor);
}

template void Safepoint::checkLivenessAndVisitChildren(AbstractSlotVisitor&);
template void Safepoint::checkLivenessAndVisitChildren(SlotVisitor&);

template<typename Visitor>
bool Safepoint::isKnownToBeLiveDuringGC(Visitor& visitor)
{
    RELEASE_ASSERT(m_didCallBegin);
    
    if (m_result.m_didGetCancelled)
        return true; // We were cancelled during a previous GC, so let's not mess with it this time around - pretend it's live and move on.
    
    return m_plan.isKnownToBeLiveDuringGC(visitor);
}

template bool Safepoint::isKnownToBeLiveDuringGC(AbstractSlotVisitor&);
template bool Safepoint::isKnownToBeLiveDuringGC(SlotVisitor&);

bool Safepoint::isKnownToBeLiveAfterGC()
{
    RELEASE_ASSERT(m_didCallBegin);
    
    if (m_result.m_didGetCancelled)
        return true; // We were cancelled during a previous GC, so let's not mess with it this time around - pretend it's live and move on.
    
    return m_plan.isKnownToBeLiveAfterGC();
}

void Safepoint::cancel()
{
    RELEASE_ASSERT(m_didCallBegin);
    RELEASE_ASSERT(!m_result.m_didGetCancelled); // We cannot get cancelled twice because subsequent GCs will think that we're alive and they will not do anything to us.
    
    RELEASE_ASSERT(m_plan.stage() == JITPlanStage::Canceled);
    m_result.m_didGetCancelled = true;
    m_vm = nullptr;
}

VM* Safepoint::vm() const
{
    return m_vm;
}

} // namespace JSC

#endif // ENABLE(JIT)

