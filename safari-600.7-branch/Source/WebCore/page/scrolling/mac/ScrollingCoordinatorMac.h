/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ScrollingCoordinatorMac_h
#define ScrollingCoordinatorMac_h

#if ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#include "AsyncScrollingCoordinator.h"

namespace WebCore {

class Scrollbar;
class ScrollingStateNode;
class ScrollingStateScrollingNode;
class ScrollingStateTree;
class ThreadedScrollingTree;

class ScrollingCoordinatorMac : public AsyncScrollingCoordinator {
public:
    explicit ScrollingCoordinatorMac(Page*);
    virtual ~ScrollingCoordinatorMac();

    virtual void pageDestroyed();

    virtual void commitTreeStateIfNeeded() override;

    // Handle the wheel event on the scrolling thread. Returns whether the event was handled or not.
    virtual bool handleWheelEvent(FrameView*, const PlatformWheelEvent&) override;

private:
    virtual void scheduleTreeStateCommit() override;

    void scrollingStateTreeCommitterTimerFired(Timer*);
    void commitTreeState();
    
    void updateTiledScrollingIndicator();

    Timer m_scrollingStateTreeCommitterTimer;
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && PLATFORM(MAC)

#endif // ScrollingCoordinatorMac_h
