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

#ifndef DOMTimer_h
#define DOMTimer_h

#include "SuspendableTimer.h"
#include <memory>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMTimerFireState;
class Document;
class Element;
class HTMLPlugInElement;
class IntRect;
class ScheduledAction;

class DOMTimer final : public RefCounted<DOMTimer>, public SuspendableTimer {
    WTF_MAKE_NONCOPYABLE(DOMTimer);
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~DOMTimer();

    static std::chrono::milliseconds defaultMinimumInterval() { return std::chrono::milliseconds(4); }
    static std::chrono::milliseconds defaultAlignmentInterval() { return std::chrono::milliseconds::zero(); }
    static std::chrono::milliseconds hiddenPageAlignmentInterval() { return std::chrono::milliseconds(1000); }

    // Creates a new timer owned by specified ScriptExecutionContext, starts it
    // and returns its Id.
    static int install(ScriptExecutionContext&, std::unique_ptr<ScheduledAction>, std::chrono::milliseconds timeout, bool singleShot);
    static void removeById(ScriptExecutionContext&, int timeoutId);

    // Notify that the interval may need updating (e.g. because the minimum interval
    // setting for the context has changed).
    void updateTimerIntervalIfNecessary();

    static void scriptDidInteractWithPlugin(HTMLPlugInElement&);

private:
    DOMTimer(ScriptExecutionContext&, std::unique_ptr<ScheduledAction>, std::chrono::milliseconds interval, bool singleShot);
    friend class Internals;

    std::chrono::milliseconds intervalClampedToMinimum() const;

    bool isDOMTimersThrottlingEnabled(Document&) const;
    void updateThrottlingStateIfNecessary(const DOMTimerFireState&);

    // SuspendableTimer
    void fired() override;
    void didStop() override;
    Optional<std::chrono::milliseconds> alignedFireTime(std::chrono::milliseconds) const override;

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
    std::chrono::milliseconds m_originalInterval;
    TimerThrottleState m_throttleState;
    std::chrono::milliseconds m_currentTimerInterval;
    bool m_shouldForwardUserGesture;
};

} // namespace WebCore

#endif // DOMTimer_h

