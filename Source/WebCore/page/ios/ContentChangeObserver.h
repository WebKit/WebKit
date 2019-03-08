/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(IOS_FAMILY)

#include "Document.h"
#include "PlatformEvent.h"
#include "RenderStyleConstants.h"
#include "Timer.h"
#include "WKContentObservation.h"
#include <wtf/HashSet.h>
#include <wtf/Seconds.h>

namespace WebCore {

class DOMTimer;
class Element;

class ContentChangeObserver {
public:
    ContentChangeObserver(Document&);

    WEBCORE_EXPORT void startContentObservationForDuration(Seconds duration);
    WEBCORE_EXPORT WKContentChange observedContentChange() const;

    void didInstallDOMTimer(const DOMTimer&, Seconds timeout, bool singleShot);
    void didRemoveDOMTimer(const DOMTimer&);
    void didSuspendActiveDOMObjects();
    void willDetachPage();

    class StyleChangeScope {
    public:
        StyleChangeScope(Document&, const Element&);
        ~StyleChangeScope();

    private:
        ContentChangeObserver& m_contentChangeObserver;
        const Element& m_element;
        bool m_needsObserving { false };
        DisplayType m_previousDisplay;
        Visibility m_previousVisibility;
        Visibility m_previousImplicitVisibility;
    };

    class TouchEventScope {
    public:
        WEBCORE_EXPORT TouchEventScope(Document&, PlatformEvent::Type);
        WEBCORE_EXPORT ~TouchEventScope();
    private:
        ContentChangeObserver& m_contentChangeObserver;
    };

    class MouseMovedScope {
    public:
        WEBCORE_EXPORT MouseMovedScope(Document&);
        WEBCORE_EXPORT ~MouseMovedScope();
    private:
        ContentChangeObserver& m_contentChangeObserver;
    };

    class StyleRecalcScope {
    public:
        StyleRecalcScope(Document&);
        ~StyleRecalcScope();
    private:
        ContentChangeObserver& m_contentChangeObserver;
    };

    class DOMTimerScope {
    public:
        DOMTimerScope(Document*, const DOMTimer&);
        ~DOMTimerScope();
    private:
        ContentChangeObserver* m_contentChangeObserver { nullptr };
        const DOMTimer& m_domTimer;
    };

private:
    void touchEventDidStart(PlatformEvent::Type);
    void touchEventDidFinish();

    void mouseMovedDidStart();
    void mouseMovedDidFinish();

    void contentVisibilityDidChange();

    void setShouldObserveDOMTimerScheduling(bool observe) { m_isObservingDOMTimerScheduling = observe; }
    bool isObservingDOMTimerScheduling() const { return m_isObservingDOMTimerScheduling; }
    void domTimerExecuteDidStart(const DOMTimer&);
    void domTimerExecuteDidFinish(const DOMTimer&);
    void registerDOMTimer(const DOMTimer& timer) { m_DOMTimerList.add(&timer); }
    void unregisterDOMTimer(const DOMTimer& timer) { m_DOMTimerList.remove(&timer); }
    void clearObservedDOMTimers() { m_DOMTimerList.clear(); }
    bool containsObservedDOMTimer(const DOMTimer& timer) const { return m_DOMTimerList.contains(&timer); }

    void styleRecalcDidStart();
    void styleRecalcDidFinish();
    void setShouldObserveNextStyleRecalc(bool);
    bool isObservingStyleRecalc() const { return m_isObservingStyleRecalc; }

    bool isObservingContentChanges() const { return m_touchEventIsBeingDispatched || m_domTimerIsBeingExecuted || m_styleRecalcIsBeingExecuted || m_contentObservationTimer.isActive(); }

    void cancelPendingActivities();

    void setHasIndeterminateState();
    void setHasVisibleChangeState();
    void setHasNoChangeState();

    bool hasVisibleChangeState() const { return observedContentChange() == WKContentVisibilityChange; }
    bool hasObservedDOMTimer() const { return !m_DOMTimerList.isEmpty(); }
    bool hasDeterminateState() const;

    bool hasPendingActivity() const { return hasObservedDOMTimer() || m_document.hasPendingStyleRecalc() || m_contentObservationTimer.isActive(); }
#if !ASSERT_DISABLED
    bool isNotifyContentChangeAllowed() const;
#endif

    void completeDurationBasedContentObservation();

    enum class Event {
        StartedTouchStartEventDispatching,
        EndedTouchStartEventDispatching,
        StartedMouseMovedEventDispatching,
        EndedMouseMovedEventDispatching,
        InstalledDOMTimer,
        RemovedDOMTimer,
        EndedDOMTimerExecution,
        StyleRecalcFinished,
        ContentVisibilityChanged,
        StartedFixedObservationTimeWindow,
        EndedFixedObservationTimeWindow
    };
    void adjustObservedState(Event);

    Document& m_document;
    Timer m_contentObservationTimer;
    HashSet<const DOMTimer*> m_DOMTimerList;
    bool m_touchEventIsBeingDispatched { false };
    bool m_isObservingStyleRecalc { false };
    bool m_styleRecalcIsBeingExecuted { false };
    bool m_isObservingDOMTimerScheduling { false };
    bool m_domTimerIsBeingExecuted { false };
    bool m_isMouseMovedPrecededByTouch { false };
#if !ASSERT_DISABLED
    bool m_mouseMovedIsBeingDispatched { false };
#endif
};

inline void ContentChangeObserver::setHasNoChangeState()
{
    WKSetObservedContentChange(WKContentNoChange);
}

inline void ContentChangeObserver::setHasIndeterminateState()
{
    ASSERT(!hasVisibleChangeState());
    WKSetObservedContentChange(WKContentIndeterminateChange);
}

inline void ContentChangeObserver::setHasVisibleChangeState()
{
    WKSetObservedContentChange(WKContentVisibilityChange);
}

}
#endif
