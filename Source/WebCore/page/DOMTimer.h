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
#include <WTF/RefCounted.h>
#include <memory>

namespace WebCore {

    class ScheduledAction;

    class DOMTimer final : public RefCounted<DOMTimer>, public SuspendableTimer {
        WTF_MAKE_NONCOPYABLE(DOMTimer);
        WTF_MAKE_FAST_ALLOCATED;
    public:
        // Creates a new timer owned by specified ScriptExecutionContext, starts it
        // and returns its Id.
        static int install(ScriptExecutionContext*, std::unique_ptr<ScheduledAction>, int timeout, bool singleShot);
        static void removeById(ScriptExecutionContext*, int timeoutId);

        // Adjust to a change in the ScriptExecutionContext's minimum timer interval.
        // This allows the minimum allowable interval time to be changed in response
        // to events like moving a tab to the background.
        void adjustMinimumTimerInterval(double oldMinimumTimerInterval);

    private:
        DOMTimer(ScriptExecutionContext*, std::unique_ptr<ScheduledAction>, int interval, bool singleShot);
        double intervalClampedToMinimum(int timeout, double minimumTimerInterval) const;

        // SuspendableTimer
        virtual void fired() override;
        virtual void didStop() override;
        virtual double alignedFireTime(double) const override;

        int m_timeoutId;
        int m_nestingLevel;
        std::unique_ptr<ScheduledAction> m_action;
        int m_originalInterval;
        bool m_shouldForwardUserGesture;
    };

} // namespace WebCore

#endif // DOMTimer_h

