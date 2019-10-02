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
#include "DocumentTimelineOptions.h"
#include "GenericTaskQueue.h"
#include "Timer.h"
#include <wtf/Markable.h>
#include <wtf/Ref.h>

namespace WebCore {

class AnimationPlaybackEvent;
class RenderElement;

class DocumentTimeline final : public AnimationTimeline
{
public:
    static Ref<DocumentTimeline> create(Document&);
    static Ref<DocumentTimeline> create(Document&, DocumentTimelineOptions&&);
    ~DocumentTimeline();

    bool isDocumentTimeline() const final { return true; }

    Vector<RefPtr<WebAnimation>> getAnimations() const;

    Document* document() const { return m_document.get(); }

    Optional<Seconds> currentTime() override;

    void animationTimingDidChange(WebAnimation&) override;
    void removeAnimation(WebAnimation&) override;
    void animationWasAddedToElement(WebAnimation&, Element&) final;
    void animationWasRemovedFromElement(WebAnimation&, Element&) final;

    // If possible, compute the visual extent of any transform animation on the given renderer
    // using the given rect, returning the result in the rect. Return false if there is some
    // transform animation but we were unable to cheaply compute its effect on the extent.
    bool computeExtentOfAnimation(RenderElement&, LayoutRect&) const;
    std::unique_ptr<RenderStyle> animatedStyleForRenderer(RenderElement& renderer);
    bool isRunningAnimationOnRenderer(RenderElement&, CSSPropertyID) const;
    bool isRunningAcceleratedAnimationOnRenderer(RenderElement&, CSSPropertyID) const;
    void animationAcceleratedRunningStateDidChange(WebAnimation&);
    void applyPendingAcceleratedAnimations();
    bool runningAnimationsForElementAreAllAccelerated(Element&) const;
    bool resolveAnimationsForElement(Element&, RenderStyle&);
    void detachFromDocument();

    void enqueueAnimationPlaybackEvent(AnimationPlaybackEvent&);
    
    void updateAnimationsAndSendEvents(DOMHighResTimeStamp timestamp);

    void updateThrottlingState();
    WEBCORE_EXPORT Seconds animationInterval() const;
    WEBCORE_EXPORT void suspendAnimations();
    WEBCORE_EXPORT void resumeAnimations();
    WEBCORE_EXPORT bool animationsAreSuspended();
    WEBCORE_EXPORT unsigned numberOfActiveAnimationsForTesting() const;
    WEBCORE_EXPORT Vector<std::pair<String, double>> acceleratedAnimationsForElement(Element&) const;    
    WEBCORE_EXPORT unsigned numberOfAnimationTimelineInvalidationsForTesting() const;

private:
    DocumentTimeline(Document&, Seconds);

    DOMHighResTimeStamp liveCurrentTime() const;
    void cacheCurrentTime(DOMHighResTimeStamp);
    void maybeClearCachedCurrentTime();
    void scheduleInvalidationTaskIfNeeded();
    void performInvalidationTask();
    void scheduleAnimationResolution();
    void unscheduleAnimationResolution();
    void internalUpdateAnimationsAndSendEvents();
    void performEventDispatchTask();
    void updateListOfElementsWithRunningAcceleratedAnimationsForElement(Element&);
    void transitionDidComplete(RefPtr<CSSTransition>);
    void scheduleNextTick();

    Timer m_tickScheduleTimer;
    GenericTaskQueue<Timer> m_currentTimeClearingTaskQueue;
    HashSet<RefPtr<WebAnimation>> m_acceleratedAnimationsPendingRunningStateChange;
    HashSet<Element*> m_elementsWithRunningAcceleratedAnimations;
    Vector<Ref<AnimationPlaybackEvent>> m_pendingAnimationEvents;
    RefPtr<Document> m_document;
    Markable<Seconds, Seconds::MarkableTraits> m_cachedCurrentTime;
    Seconds m_originTime;
    unsigned m_numberOfAnimationTimelineInvalidationsForTesting { 0 };
    bool m_isSuspended { false };
    bool m_waitingOnVMIdle { false };
    bool m_animationResolutionScheduled { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_ANIMATION_TIMELINE(DocumentTimeline, isDocumentTimeline())
