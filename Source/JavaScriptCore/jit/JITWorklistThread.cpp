/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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
#include "JITWorklistThread.h"

#if ENABLE(JIT)

#include "HeapInlines.h"
#include "JITWorklist.h"
#include "VM.h"
#include <wtf/TZoneMallocInlines.h>

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JITWorkerSlot);
WTF_MAKE_TZONE_ALLOCATED_IMPL(JITWorklistThread);

#if USE(PROTECTED_JIT_STACKS)
JITWorklistThread::JITWorklistThread(const AbstractLocker& locker, JITWorklist& worklist, JITWorkerSlot& slot)
    : SequesteredAutomaticThread(locker, worklist.m_lock, worklist.m_planEnqueued.copyRef())
    , m_worklist(worklist)
    , m_slot(slot)
{
}
#else
JITWorklistThread::JITWorklistThread(const AbstractLocker& locker, JITWorklist& worklist, JITWorkerSlot& slot)
    : AutomaticThread(locker, worklist.m_lock, worklist.m_planEnqueued.copyRef(), ThreadType::Compiler, compilerThreadQOS())
    , m_worklist(worklist)
    , m_slot(slot)
{
}
#endif

ASCIILiteral JITWorklistThread::name() const
{
#if OS(LINUX)
    return "JITWorker"_s;
#else
    return "JIT Worklist Helper Thread"_s;
#endif
}

auto JITWorklistThread::poll(const AbstractLocker& locker) -> PollResult
{
    if (m_worklist.dequeuePlan(locker, m_slot.get()))
        return PollResult::Work;
    RELEASE_ASSERT(m_worklist.m_numberOfActiveThreads);
    m_worklist.m_numberOfActiveThreads--;
    return PollResult::Wait;
}

auto JITWorklistThread::work() -> WorkResult
{
    m_worklist.compileOnSlot(m_slot.get());
    return WorkResult::Continue;
}

void JITWorklistThread::threadDidStart()
{
    dataLogLnIf(Options::verboseCompilationQueue(), m_worklist, ": Thread started");

}

void JITWorklistThread::threadIsStopping(const AbstractLocker&)
{
    dataLogLnIf(Options::verboseCompilationQueue(), m_worklist, ": Thread will stop");
    ASSERT(!m_slot->m_plan);
}

} // namespace JSC

#endif // ENABLE(JIT)
