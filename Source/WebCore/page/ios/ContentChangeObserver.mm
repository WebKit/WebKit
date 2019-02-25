/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ContentChangeObserver.h"

#if PLATFORM(IOS_FAMILY)
#import "DOMTimer.h"
#import "WKContentObservationInternal.h"

namespace WebCore {

void ContentChangeObserver::startObservingContentChanges()
{
    WKStartObservingContentChanges();
}

void ContentChangeObserver::stopObservingContentChanges()
{
    WKStopObservingContentChanges();   
}

bool ContentChangeObserver::isObservingContentChanges()
{
    return WKObservingContentChanges();   
}

void ContentChangeObserver::startObservingDOMTimerScheduling()
{
    WKStartObservingDOMTimerScheduling();
}

void ContentChangeObserver::stopObservingDOMTimerScheduling()
{
    WKStopObservingDOMTimerScheduling();
}

bool ContentChangeObserver::isObservingDOMTimerScheduling()
{
    return WKIsObservingDOMTimerScheduling();
}

void ContentChangeObserver::startObservingStyleRecalcScheduling()
{
    WKStartObservingStyleRecalcScheduling();
}

void ContentChangeObserver::stopObservingStyleRecalcScheduling()
{
    WKStopObservingStyleRecalcScheduling();
}

bool ContentChangeObserver::isObservingStyleRecalcScheduling()
{
    return WKIsObservingStyleRecalcScheduling();
}

void ContentChangeObserver::setShouldObserveNextStyleRecalc(bool observe)
{
    WKSetShouldObserveNextStyleRecalc(observe);
}

bool ContentChangeObserver::shouldObserveNextStyleRecalc()
{
    return WKShouldObserveNextStyleRecalc();
}

WKContentChange ContentChangeObserver::observedContentChange()
{
    return WKObservedContentChange();
}

unsigned ContentChangeObserver::countOfObservedDOMTimers()
{
    return WebThreadCountOfObservedDOMTimers();
}

void ContentChangeObserver::clearObservedDOMTimers()
{
    WebThreadClearObservedDOMTimers();
}

void ContentChangeObserver::setObservedContentChange(WKContentChange change)
{
    WKSetObservedContentChange(change);
}

bool ContentChangeObserver::containsObservedDOMTimer(DOMTimer& timer)
{
    return WebThreadContainsObservedDOMTimer(&timer);
}

void ContentChangeObserver::addObservedDOMTimer(DOMTimer& timer)
{
    WebThreadAddObservedDOMTimer(&timer);
}

void ContentChangeObserver::removeObservedDOMTimer(DOMTimer& timer)
{
    WebThreadRemoveObservedDOMTimer(&timer);
}

}

#endif // PLATFORM(IOS_FAMILY)
