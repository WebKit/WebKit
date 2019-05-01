/*
 * Copyright (C) 2008, 2014 Apple Inc. All Rights Reserved.
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

#include "SuspendableTimer.h"
#include "UserGestureIndicator.h"
#include <memory>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>

namespace WebCore {

class DOMTimerFireState;
class Document;
class HTMLPlugInElement;
class ScheduledAction;

class DOMTimer final : public RefCounted<DOMTimer>, public SuspendableTimer {
    WTF_MAKE_NONCOPYABLE(DOMTimer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DOMTimer();

    static Seconds defaultMinimumInterval() { return 4_ms; }
    static Seconds defaultAlignmentInterval() { return 0_s; }
    static Seconds defaultAlignmentIntervalInLowPowerMode() { return 30_ms; }
    static Seconds nonInteractedCrossOriginFrameAlignmentInterval() { return 30_ms; }
    static Seconds hiddenPageAlignmentInterval() { return 1_s; }

    // Creates a new timer owned by specified ScriptExecutionContext, starts it
    // and returns its Id.
    static int install(ScriptExecutionContext&, std::unique_ptr<ScheduledAction>, Seconds timeout, bool singleShot);
    static void removeById(ScriptExecutionContext&, int timeoutId);

    // Notify that the interval may need updating (e.g. because the minimum interval
    // setting for the context has changed).
    void updateTimerIntervalIfNecessary();

    static void scriptDidInteractWithPlugin(HTMLPlugInElement&);

private:
    DOMTimer(ScriptExecutionContext&, std::unique_ptr<ScheduledAction>, Seconds interval, bool singleShot);
    friend class Internals;

    WEBCORE_EXPORT Seconds intervalClampedToMinimum() const;

    bool isDOMTimersThrottlingEnabled(Document&) const;
    void updateThrottlingStateIfNecessary(const DOMTimerFireState&);

    // SuspendableTimer
    void fired() override;
    void didStop() override;
    WEBCORE_EXPORT Optional<MonotonicTime> alignedFireTime(MonotonicTime) const override;

    // ActiveDOMObject API.
    const char* activeDOMObjectName() const override;

    enum TimerThrottleState {
        Undetermined,
        ShouldThrottle,
        ShouldNotThrottle
    };

    int m_timeoutId;
    int m_nestingLevel;
    std::unique_ptr<ScheduledAction> m_action;
    Seconds m_originalInterval;
    TimerThrottleState m_throttleState;
    Seconds m_currentTimerInterval;
    RefPtr<UserGestureToken> m_userGestureTokenToForward;
};

} // namespace WebCore
