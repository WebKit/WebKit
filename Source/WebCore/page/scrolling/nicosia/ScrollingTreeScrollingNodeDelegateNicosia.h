/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 * Copyright (C) 2019, 2021 Igalia S.L.
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

#include "ScrollingTreeScrollingNodeDelegate.h"

#if ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)

#include "NicosiaPlatformLayer.h"
#include "ScrollAnimation.h"
#include "ScrollingStateOverflowScrollingNode.h"
#include "ThreadedScrollingTree.h"

#if ENABLE(KINETIC_SCROLLING)
#include "ScrollAnimationKinetic.h"
#endif

#if ENABLE(SMOOTH_SCROLLING)
#include "ScrollAnimationSmooth.h"
#endif

#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
#include <wtf/RunLoop.h>
#endif

namespace WebCore {

// FIXME: This should not be a ScrollAnimationClient.
class ScrollingTreeScrollingNodeDelegateNicosia : public ScrollingTreeScrollingNodeDelegate, public ScrollAnimationClient {
public:
    explicit ScrollingTreeScrollingNodeDelegateNicosia(ScrollingTreeScrollingNode&, bool scrollAnimatorEnabled);
    virtual ~ScrollingTreeScrollingNodeDelegateNicosia();

    std::unique_ptr<Nicosia::SceneIntegration::UpdateScope> createUpdateScope();
    void resetCurrentPosition();
    void updateVisibleLengths();
    WheelEventHandlingResult handleWheelEvent(const PlatformWheelEvent&, EventTargeting);
    void stopScrollAnimations();

private:
    bool m_scrollAnimatorEnabled { false };
    float pageScaleFactor();

#if ENABLE(KINETIC_SCROLLING)
    void ensureScrollAnimationKinetic();
#endif
#if ENABLE(SMOOTH_SCROLLING)
    void ensureScrollAnimationSmooth();
#endif

    void animationTimerFired();
    void startTimerIfNecessary();

    // ScrollAnimationClient
    void scrollAnimationDidUpdate(ScrollAnimation&, const FloatPoint& currentPosition) final;
    void scrollAnimationDidEnd(ScrollAnimation&) final;
    ScrollExtents scrollExtentsForAnimation(ScrollAnimation&) final;

    // FIXME: These animations should not live here. They need to be managed by ScrollingEffectsController,
    // to be coordinated with other kinds of scroll animations, and be referenced by ScrollingEffectsController::m_currentAnimation.
    
#if ENABLE(KINETIC_SCROLLING)
    std::unique_ptr<ScrollAnimationKinetic> m_kineticAnimation;
#endif
#if ENABLE(SMOOTH_SCROLLING)
    std::unique_ptr<ScrollAnimationSmooth> m_smoothAnimation;
#endif

#if ENABLE(KINETIC_SCROLLING) || ENABLE(SMOOTH_SCROLLING)
    // FIXME: When the above two animations are removed, this timer can be removed.
    RunLoop::Timer<ScrollingTreeScrollingNodeDelegateNicosia> m_animationTimer;
#endif
};

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING) && USE(NICOSIA)
