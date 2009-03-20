/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "ActiveDOMObject.h"
#include "Timer.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class ScheduledAction;

class DOMTimer : public TimerBase, public ActiveDOMObject {
public:
    virtual ~DOMTimer();
    // Creates a new timer owned by specified ScriptExecutionContext, starts it
    // and returns its Id.
    static int install(ScriptExecutionContext*, ScheduledAction*, int timeout, bool singleShot);
    static void removeById(ScriptExecutionContext*, int timeoutId);

    // ActiveDOMObject
    virtual bool hasPendingActivity() const;
    virtual void contextDestroyed();
    virtual void stop();
    virtual bool canSuspend() const;
    virtual void suspend();
    virtual void resume();

    // The lowest allowable timer setting (in seconds, 0.001 == 1 ms).
    // Default is 10ms.
    // Chromium uses a non-default timeout.
    static double minTimerInterval() { return s_minTimerInterval; }
    static void setMinTimerInterval(double value) { s_minTimerInterval = value; }

private:
    DOMTimer(ScriptExecutionContext*, ScheduledAction*, int timeout, bool singleShot);
    virtual void fired();

    int m_timeoutId;
    int m_nestingLevel;
    OwnPtr<ScheduledAction> m_action;
    double m_nextFireInterval;
    double m_repeatInterval;
    static double s_minTimerInterval;
};

} // namespace WebCore

#endif // DOMTimer_h

