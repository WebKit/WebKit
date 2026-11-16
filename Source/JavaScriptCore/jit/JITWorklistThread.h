/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(JIT)

#include "JITPlan.h"
#include <wtf/AutomaticThread.h>
#include <wtf/Platform.h>
#include <wtf/SequesteredAutomaticThread.h>

namespace JSC {

class JITWorklist;
class Safepoint;

// The state of one in-flight compile. The set of slots is fixed, so the GC can enumerate them.
class JITWorkerSlot final : public CanMakeThreadSafeCheckedPtr<JITWorkerSlot> {
    WTF_MAKE_NONCOPYABLE(JITWorkerSlot);
    WTF_MAKE_TZONE_ALLOCATED(JITWorkerSlot);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(JITWorkerSlot);

    friend class JITWorklist;
    friend class JITWorklistThread;
    friend class Safepoint;

public:
    JITWorkerSlot() = default;

    const Safepoint* safepoint() const { return m_safepoint; }

private:
    // The GC suspends compiles by locking every slot's m_rightToRun, and a compile
    // releases it only inside a Safepoint.
    Lock m_rightToRun;
    RefPtr<JITPlan> m_plan { nullptr };
    unsigned m_planLoad { 0 };
    Safepoint* m_safepoint { nullptr };
    // A pool thread owns this slot until its WorkScope ends, which is after m_plan has been handed off.
    bool m_isInUse { false };
};

#if USE(PROTECTED_JIT_STACKS)
using JITWorklistThreadBase = SequesteredAutomaticThread;
#else
using JITWorklistThreadBase = AutomaticThread;
#endif

class JITWorklistThread final : public JITWorklistThreadBase {
    WTF_MAKE_TZONE_ALLOCATED(JITWorklistThread);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(JITWorklistThread);

public:
    JITWorklistThread(const AbstractLocker&, JITWorklist&, JITWorkerSlot&);

    ASCIILiteral name() const final;

private:
    PollResult poll(const AbstractLocker&) final;
    WorkResult work() final;

    void threadDidStart() final;

    void threadIsStopping(const AbstractLocker&) final;

    JITWorklist& m_worklist;
    const CheckedRef<JITWorkerSlot> m_slot;
};

} // namespace JSC

#endif // ENABLE(JIT)
