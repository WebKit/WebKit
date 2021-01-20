/*
 * Copyright (C) 2008, 2013 Apple Inc. All Rights Reserved.
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
 *
 */

#pragma once

#include "ActiveDOMObject.h"
#include "Timer.h"

#include <wtf/Function.h>
#include <wtf/Seconds.h>

namespace WebCore {

class SuspendableTimerBase : private TimerBase, public ActiveDOMObject {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit SuspendableTimerBase(ScriptExecutionContext*);
    virtual ~SuspendableTimerBase();

    // A hook for derived classes to perform cleanup.
    virtual void didStop();

    // Part of TimerBase interface used by SuspendableTimerBase clients, modified to work when suspended.
    bool isActive() const { return TimerBase::isActive() || (m_suspended && m_savedIsActive); }
    bool isSuspended() const { return m_suspended; }

    Seconds repeatInterval() const;

    void startRepeating(Seconds repeatInterval);
    void startOneShot(Seconds interval);

    void augmentFireInterval(Seconds delta);
    void augmentRepeatInterval(Seconds delta);

    using TimerBase::didChangeAlignmentInterval;
    using TimerBase::operator new;
    using TimerBase::operator delete;

    void cancel(); // Equivalent to TimerBase::stop(), whose name conflicts with ActiveDOMObject::stop().

private:
    void fired() override = 0;

    // ActiveDOMObject API.
    bool virtualHasPendingActivity() const final;
    void stop() final;
    void suspend(ReasonForSuspension) final;
    void resume() final;

    Seconds m_savedNextFireInterval;
    Seconds m_savedRepeatInterval;

    bool m_suspended { false };
    bool m_savedIsActive { false };
};

class SuspendableTimer final : public SuspendableTimerBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    template <typename TimerFiredClass, typename TimerFiredBaseClass>
    SuspendableTimer(ScriptExecutionContext* context, TimerFiredClass& object, void (TimerFiredBaseClass::*function)())
        : SuspendableTimerBase(context)
        , m_function(std::bind(function, &object))
    {
    }

    SuspendableTimer(ScriptExecutionContext* context, WTF::Function<void ()>&& function)
        : SuspendableTimerBase(context)
        , m_function(WTFMove(function))
    {
    }

private:
    void fired() final
    {
        m_function();
    }

    const char* activeDOMObjectName() const final;

    WTF::Function<void ()> m_function;
};

} // namespace WebCore
