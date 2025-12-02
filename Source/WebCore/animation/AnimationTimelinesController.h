/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <WebCore/FrameRateAligner.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ReducedResolutionSeconds.h>
#include <WebCore/Timer.h>
#include <WebCore/WebAnimationTypes.h>
#include <wtf/CancellableTask.h>
#include <wtf/CheckedRef.h>
#include <wtf/Markable.h>
#include <wtf/Seconds.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {

class AnimationTimeline;
class Document;
class ScrollTimeline;
class WeakPtrImplWithEventTargetData;
class WebAnimation;

struct Styleable;

#if ENABLE(THREADED_ANIMATIONS)
class AcceleratedEffectStackUpdater;
#endif

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AnimationTimelinesController);
class AnimationTimelinesController final : public CanMakeCheckedPtr<AnimationTimelinesController> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AnimationTimelinesController, AnimationTimelinesController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AnimationTimelinesController);
public:
    explicit AnimationTimelinesController(Document&);
    ~AnimationTimelinesController();

    void addTimeline(AnimationTimeline&);
    void removeTimeline(AnimationTimeline&);
    void detachFromDocument();
    void updateAnimationsAndSendEvents(ReducedResolutionSeconds);
    void runPostRenderingUpdateTasks();
    void addPendingAnimation(WebAnimation&);

    std::optional<Seconds> currentTime(UseCachedCurrentTime = UseCachedCurrentTime::Yes);
    std::optional<FramesPerSecond> maximumAnimationFrameRate() const { return m_frameRateAligner.maximumFrameRate(); }
    std::optional<Seconds> timeUntilNextTickForAnimationsWithFrameRate(FramesPerSecond) const;

    WEBCORE_EXPORT void suspendAnimations();
    WEBCORE_EXPORT void resumeAnimations();
    bool animationsAreSuspended() const { return m_isSuspended; }

#if ENABLE(THREADED_ANIMATIONS)
    AcceleratedEffectStackUpdater* existingAcceleratedEffectStackUpdater() const { return m_acceleratedEffectStackUpdater.get(); }
    void scheduleAcceleratedEffectStackUpdateForTarget(const Styleable&);
#endif

    WEBCORE_EXPORT Vector<GraphicsLayer::AcceleratedAnimationForTesting> acceleratedAnimationsForElement(const Element&) const;

private:

    ReducedResolutionSeconds liveCurrentTime() const;
    void cacheCurrentTime(ReducedResolutionSeconds);
    void clearCachedCurrentTime();
    void processPendingAnimations();
    bool isPendingTimelineAttachment(const WebAnimation&) const;

    Ref<Document> protectedDocument() const { return m_document.get(); }

#if ENABLE(THREADED_ANIMATIONS)
    std::unique_ptr<AcceleratedEffectStackUpdater> m_acceleratedEffectStackUpdater;
#endif

    Timer m_cachedCurrentTimeClearanceTimer;
    Vector<Ref<ScrollTimeline>> m_updatedScrollTimelines;
    HashMap<FramesPerSecond, ReducedResolutionSeconds> m_animationFrameRateToLastTickTimeMap;
    WeakHashSet<AnimationTimeline> m_timelines;
    WeakHashSet<WebAnimation, WeakPtrImplWithEventTargetData> m_pendingAnimations;
    TaskCancellationGroup m_pendingAnimationsProcessingTaskCancellationGroup;
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_document;
    FrameRateAligner m_frameRateAligner;
    Markable<Seconds> m_cachedCurrentTime;
    bool m_isSuspended { false };
};

} // namespace WebCore
