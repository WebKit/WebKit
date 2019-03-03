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

#include "WKContentObservation.h"

namespace WebCore {

class DOMTimer;
class Page;

class ContentChangeObserver {
public:
    ContentChangeObserver(Page&);

    WEBCORE_EXPORT void startObservingContentChanges();
    WEBCORE_EXPORT void stopObservingContentChanges();
    WEBCORE_EXPORT WKContentChange observedContentChange() const;

    void didInstallDOMTimer(const DOMTimer&, Seconds timeout, bool singleShot);
    void didRemoveDOMTimer(const DOMTimer&);
    void contentVisibilityDidChange();
    void didSuspendActiveDOMObjects();
    void willDetachPage();

    class StyleChangeScope {
    public:
        StyleChangeScope(Page*, const Element&);
        ~StyleChangeScope();

    private:
        ContentChangeObserver* m_contentChangeObserver { nullptr };
        const Element& m_element;
        bool m_needsObserving { false };
        DisplayType m_previousDisplay;
        Visibility m_previousVisibility;
        Visibility m_previousImplicitVisibility;
    };

    class StyleRecalcScope {
    public:
        StyleRecalcScope(Page*);
        ~StyleRecalcScope();
    private:
        ContentChangeObserver* m_contentChangeObserver { nullptr };
    };

    class DOMTimerScope {
    public:
        DOMTimerScope(Page*, const DOMTimer&);
        ~DOMTimerScope();
    private:
        ContentChangeObserver* m_contentChangeObserver { nullptr };
        const DOMTimer& m_domTimer;
    };

private:
    void startObservingDOMTimerScheduling() { m_isObservingDOMTimerScheduling = true; }
    void stopObservingDOMTimerScheduling() { m_isObservingDOMTimerScheduling = false; }

    void startObservingDOMTimerExecute(const DOMTimer&);
    void stopObservingDOMTimerExecute(const DOMTimer&);

    void startObservingStyleRecalc();
    void stopObservingStyleRecalc();

    void registerDOMTimer(const DOMTimer&);
    void unregisterDOMTimer(const DOMTimer&);
    bool isObservingDOMTimerScheduling() const { return m_isObservingDOMTimerScheduling; }
    bool containsObservedDOMTimer(const DOMTimer& timer) const { return m_DOMTimerList.contains(&timer); }

    void setShouldObserveStyleRecalc(bool);
    bool shouldObserveStyleRecalc() const { return m_shouldObserveStyleRecalc; }

    bool isObservingContentChanges() const { return m_isObservingContentChanges; }

    void clearObservedDOMTimers() { m_DOMTimerList.clear(); }
    void clearTimersAndReportContentChange();

    void setHasIndeterminateState();
    void setHasVisibleChangeState();
    void setHasNoChangeState();

    bool hasVisibleChangeState() const { return observedContentChange() == WKContentVisibilityChange; }
    bool hasObservedDOMTimer() const { return !m_DOMTimerList.isEmpty(); }
    bool hasDeterminateState() const;

    void notifyContentChangeIfNeeded();

    enum class Event {
        ContentObservationStarted,
        InstalledDOMTimer,
        RemovedDOMTimer,
        StyleRecalcFinished,
        ContentVisibilityChanged
    };
    void adjustObservedState(Event);

    Page& m_page;
    HashSet<const DOMTimer*> m_DOMTimerList;
    bool m_shouldObserveStyleRecalc { false };
    bool m_isObservingDOMTimerScheduling { false };
    bool m_isObservingContentChanges { false };
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
