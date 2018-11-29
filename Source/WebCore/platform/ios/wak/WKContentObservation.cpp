/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKContentObservation.h"
#include "WKContentObservationInternal.h"

#if PLATFORM(IOS_FAMILY)

#include "JSDOMBinding.h"
#include "WebCoreThread.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

WKContentChange _WKContentChange                    = WKContentNoChange;
bool            _WKObservingContentChanges          = false;
bool            _WKObservingDOMTimerScheduling      = false;


bool WKObservingContentChanges(void)
{
    return _WKObservingContentChanges;
}

void WKStartObservingContentChanges()
{
    _WKContentChange = WKContentNoChange;
    _WKObservingContentChanges = true;
}

void WKStopObservingContentChanges(void)
{
    _WKObservingContentChanges = false;
}


void WKStartObservingDOMTimerScheduling(void)
{
    _WKObservingDOMTimerScheduling = true;
    WebThreadClearObservedDOMTimers();
}

void WKStopObservingDOMTimerScheduling(void)
{
    _WKObservingDOMTimerScheduling = false;
}

WKContentChange WKObservedContentChange(void)
{
    return _WKContentChange;
}

void WKSetObservedContentChange(WKContentChange aChange)
{
    if (aChange > _WKContentChange && (_WKObservingDOMTimerScheduling || aChange != WKContentIndeterminateChange)) {
        _WKContentChange = aChange;
        if (_WKContentChange == WKContentVisibilityChange)
            WebThreadClearObservedDOMTimers();
    }
}

using DOMTimerList = HashSet<void*>;
static DOMTimerList& WebThreadGetObservedDOMTimers()
{
    ASSERT(WebThreadIsLockedOrDisabled());
    static NeverDestroyed<DOMTimerList> observedDOMTimers;
    return observedDOMTimers;
}

int WebThreadCountOfObservedDOMTimers(void)
{
    return WebThreadGetObservedDOMTimers().size();
}

void WebThreadClearObservedDOMTimers()
{
    WebThreadGetObservedDOMTimers().clear();
}

bool WebThreadContainsObservedDOMTimer(void* timer)
{
    return WebThreadGetObservedDOMTimers().contains(timer);
}

void WebThreadAddObservedDOMTimer(void* timer)
{
    if (_WKContentChange != WKContentVisibilityChange && _WKObservingDOMTimerScheduling)
        WebThreadGetObservedDOMTimers().add(timer);
}

void WebThreadRemoveObservedDOMTimer(void* timer)
{
    WebThreadGetObservedDOMTimers().remove(timer);
    // Force reset the content change flag when the last observed content modifier is removed. We should not be in indeterminate state anymore.
    if (WebThreadCountOfObservedDOMTimers())
        return;
    if (WKObservedContentChange() != WKContentIndeterminateChange)
        return;
    _WKContentChange = WKContentNoChange;
}

#endif // PLATFORM(IOS_FAMILY)
