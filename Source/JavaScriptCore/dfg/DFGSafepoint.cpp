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

#if ENABLE(DFG_JIT)

#include "DFGSafepoint.h"

#include "DFGPlan.h"
#include "DFGScannable.h"
#include "DFGThreadData.h"
#include "Operations.h"

namespace JSC { namespace DFG {

Safepoint::Safepoint(Plan& plan)
    : m_plan(plan)
    , m_didCallBegin(false)
{
}

Safepoint::~Safepoint()
{
    RELEASE_ASSERT(m_didCallBegin);
    if (ThreadData* data = m_plan.threadData) {
        RELEASE_ASSERT(data->m_safepoint == this);
        data->m_safepoint = nullptr;
        data->m_rightToRun.lock();
    }
}

void Safepoint::add(Scannable* scannable)
{
    RELEASE_ASSERT(!m_didCallBegin);
    m_scannables.append(scannable);
}

void Safepoint::begin()
{
    RELEASE_ASSERT(!m_didCallBegin);
    if (ThreadData* data = m_plan.threadData) {
        RELEASE_ASSERT(!data->m_safepoint);
        data->m_safepoint = this;
        data->m_rightToRun.unlock();
    }
    m_didCallBegin = true;
}

void Safepoint::visitChildren(SlotVisitor& visitor)
{
    RELEASE_ASSERT(m_didCallBegin);
    for (unsigned i = m_scannables.size(); i--;)
        m_scannables[i]->visitChildren(visitor);
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

