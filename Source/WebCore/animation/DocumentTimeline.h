/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 */

#pragma once

#include "AnimationTimeline.h"
#include "GenericTaskQueue.h"
#include "PlatformScreen.h"
#include "Timer.h"
#include <wtf/Ref.h>

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
#include "DisplayRefreshMonitorClient.h"
#endif

namespace WebCore {

class AnimationPlaybackEvent;
class RenderElement;

class DocumentTimeline final : public AnimationTimeline
#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    , public DisplayRefreshMonitorClient
#endif
{
public:
    static Ref<DocumentTimeline> create(Document&, PlatformDisplayID);
    ~DocumentTimeline();

    std::optional<Seconds> currentTime() override;
    void pause() override;

    void animationTimingModelDidChange() override;
    void windowScreenDidChange(PlatformDisplayID);

    std::unique_ptr<RenderStyle> animatedStyleForRenderer(RenderElement& renderer);
    void animationAcceleratedRunningStateDidChange(WebAnimation&);
    bool runningAnimationsForElementAreAllAccelerated(Element&);
    void detachFromDocument();

    void enqueueAnimationPlaybackEvent(AnimationPlaybackEvent&);

private:
    DocumentTimeline(Document&, PlatformDisplayID);

    void scheduleInvalidationTaskIfNeeded();
    void performInvalidationTask();
    void updateAnimationSchedule();
    void animationScheduleTimerFired();
    void scheduleAnimationResolution();
    void updateAnimations();
    void performEventDispatchTask();

    RefPtr<Document> m_document;
    bool m_paused { false };
    std::optional<Seconds> m_cachedCurrentTime;
    GenericTaskQueue<Timer> m_invalidationTaskQueue;
    GenericTaskQueue<Timer> m_eventDispatchTaskQueue;
    bool m_needsUpdateAnimationSchedule { false };
    Timer m_animationScheduleTimer;
    HashSet<RefPtr<WebAnimation>> m_acceleratedAnimationsPendingRunningStateChange;
    Vector<Ref<AnimationPlaybackEvent>> m_pendingAnimationEvents;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    // Override for DisplayRefreshMonitorClient
    void displayRefreshFired() override;
    RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const override;
#else
    void animationResolutionTimerFired();
    Timer m_animationResolutionTimer;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(DocumentTimeline, isDocumentTimeline())
