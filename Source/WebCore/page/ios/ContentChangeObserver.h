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

    void didInstallDOMTimer(const DOMTimer&, Seconds timeout, bool singleShot);
    void removeDOMTimer(const DOMTimer&);
    void startObservingDOMTimerExecute(const DOMTimer&);
    void stopObservingDOMTimerExecute(const DOMTimer&);

    void didScheduleStyleRecalc();
    void startObservingStyleResolve();
    void stopObservingStyleResolve();

    void didSuspendActiveDOMObjects();
    void willDetachPage();

    WEBCORE_EXPORT void startObservingContentChanges();
    WEBCORE_EXPORT void stopObservingContentChanges();

    WEBCORE_EXPORT WKContentChange observedContentChange();

    class StyleChange {
    public:
        StyleChange(const Element&, ContentChangeObserver&);
        ~StyleChange();

    private:
        const Element& m_element;
        ContentChangeObserver& m_contentChangeObserver;
        DisplayType m_previousDisplay;
        Visibility m_previousVisibility;
        Visibility m_previousImplicitVisibility;
    };

private:
    void startObservingDOMTimerScheduling();
    void stopObservingDOMTimerScheduling();

    void addObservedDOMTimer(const DOMTimer&);
    bool isObservingDOMTimerScheduling();
    void removeObservedDOMTimer(const DOMTimer&);
    bool containsObservedDOMTimer(const DOMTimer&);

    void startObservingStyleRecalcScheduling();
    void stopObservingStyleRecalcScheduling();

    void setShouldObserveNextStyleRecalc(bool);
    bool shouldObserveNextStyleRecalc();

    bool isObservingContentChanges();
    bool isObservingStyleRecalcScheduling();

    void setObservedContentChange(WKContentChange);

    unsigned countOfObservedDOMTimers();
    void clearObservedDOMTimers();

    void clearTimersAndReportContentChange();

    Page& m_page;
    HashSet<const DOMTimer*> m_DOMTimerList;
};

}
#endif
