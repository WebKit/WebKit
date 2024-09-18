/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GestureRecognizerConsistencyEnforcer.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "WKContentViewInteraction.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(GestureRecognizerConsistencyEnforcer);

GestureRecognizerConsistencyEnforcer::GestureRecognizerConsistencyEnforcer(WKContentView *view)
    : m_view(view)
    , m_timer(RunLoop::main(), this, &GestureRecognizerConsistencyEnforcer::timerFired)
{
    ASSERT(m_view);
}

GestureRecognizerConsistencyEnforcer::~GestureRecognizerConsistencyEnforcer() = default;

void GestureRecognizerConsistencyEnforcer::beginTracking(WKDeferringGestureRecognizer *gesture)
{
    m_timer.stop();
    m_deferringGestureRecognizersWithTouches.add(gesture);
}

void GestureRecognizerConsistencyEnforcer::endTracking(WKDeferringGestureRecognizer *gesture)
{
    if (!m_deferringGestureRecognizersWithTouches.remove(gesture))
        return;

    if (m_deferringGestureRecognizersWithTouches.isEmpty())
        m_timer.startOneShot(1_s);
}

void GestureRecognizerConsistencyEnforcer::reset()
{
    m_timer.stop();
    m_deferringGestureRecognizersWithTouches.clear();
}

void GestureRecognizerConsistencyEnforcer::timerFired()
{
    if (!m_view)
        return;

    auto strongView = m_view.get();
    auto possibleDeferringGestures = [NSMutableArray<WKDeferringGestureRecognizer *> array];
    for (WKDeferringGestureRecognizer *gesture in [strongView deferringGestures]) {
        if (gesture.state == UIGestureRecognizerStatePossible && gesture.enabled)
            [possibleDeferringGestures addObject:gesture];
    }

    if (!possibleDeferringGestures.count)
        return;

    auto touchEventState = [strongView touchEventGestureRecognizer].state;
    if (touchEventState == UIGestureRecognizerStatePossible || touchEventState == UIGestureRecognizerStateBegan || touchEventState == UIGestureRecognizerStateChanged)
        return;

    for (WKDeferringGestureRecognizer *gesture in possibleDeferringGestures)
        [gesture setState:UIGestureRecognizerStateEnded];

    RELEASE_LOG_FAULT(ViewGestures, "Touch event gesture recognizer failed to reset after ending gesture deferral: %@", possibleDeferringGestures);
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
