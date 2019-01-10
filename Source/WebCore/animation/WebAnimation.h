/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "DOMPromiseProxy.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "WebAnimationUtilities.h"
#include <wtf/Forward.h>
#include <wtf/Markable.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationEffect;
class AnimationPlaybackEvent;
class AnimationTimeline;
class Document;
class Element;
class RenderStyle;

class WebAnimation : public RefCounted<WebAnimation>, public CanMakeWeakPtr<WebAnimation>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<WebAnimation> create(Document&, AnimationEffect*);
    static Ref<WebAnimation> create(Document&, AnimationEffect*, AnimationTimeline*);
    ~WebAnimation();

    virtual bool isDeclarativeAnimation() const { return false; }
    virtual bool isCSSAnimation() const { return false; }
    virtual bool isCSSTransition() const { return false; }

    const String& id() const { return m_id; }
    void setId(const String& id) { m_id = id; }

    AnimationEffect* effect() const { return m_effect.get(); }
    void setEffect(RefPtr<AnimationEffect>&&);
    AnimationTimeline* timeline() const { return m_timeline.get(); }
    virtual void setTimeline(RefPtr<AnimationTimeline>&&);

    Optional<Seconds> currentTime() const;
    ExceptionOr<void> setCurrentTime(Optional<Seconds>);

    enum class Silently : uint8_t { Yes, No };
    double playbackRate() const { return m_playbackRate + 0; }
    void setPlaybackRate(double);

    enum class PlayState : uint8_t { Idle, Running, Paused, Finished };
    PlayState playState() const;

    bool pending() const { return hasPendingPauseTask() || hasPendingPlayTask(); }

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<WebAnimation>>;
    ReadyPromise& ready() { return m_readyPromise.get(); }

    using FinishedPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<WebAnimation>>;
    FinishedPromise& finished() { return m_finishedPromise.get(); }

    virtual void cancel();
    void cancel(Silently);
    ExceptionOr<void> finish();
    ExceptionOr<void> play();
    void updatePlaybackRate(double);
    ExceptionOr<void> pause();
    ExceptionOr<void> reverse();

    virtual Optional<double> startTime() const;
    virtual void setStartTime(Optional<double>);
    virtual Optional<double> bindingsCurrentTime() const;
    virtual ExceptionOr<void> setBindingsCurrentTime(Optional<double>);
    virtual PlayState bindingsPlayState() const { return playState(); }
    virtual bool bindingsPending() const { return pending(); }
    virtual ReadyPromise& bindingsReady() { return ready(); }
    virtual FinishedPromise& bindingsFinished() { return finished(); }
    virtual ExceptionOr<void> bindingsPlay() { return play(); }
    virtual ExceptionOr<void> bindingsPause() { return pause(); }

    virtual bool needsTick() const;
    virtual void tick();
    Seconds timeToNextTick() const;
    virtual void resolve(RenderStyle&);
    void effectTargetDidChange(Element* previousTarget, Element* newTarget);
    void acceleratedStateDidChange();
    void applyPendingAcceleratedActions();

    bool isRunningAccelerated() const;
    bool isRelevant() const { return m_isRelevant; }
    void effectTimingDidChange();
    void suspendEffectInvalidation();
    void unsuspendEffectInvalidation();
    void setSuspended(bool);
    bool isSuspended() const { return m_isSuspended; }
    virtual void remove();

    using RefCounted::ref;
    using RefCounted::deref;

protected:
    explicit WebAnimation(Document&);

    void stop() override;

private:
    enum class DidSeek : uint8_t { Yes, No };
    enum class SynchronouslyNotify : uint8_t { Yes, No };
    enum class RespectHoldTime : uint8_t { Yes, No };
    enum class AutoRewind : uint8_t { Yes, No };
    enum class TimeToRunPendingTask : uint8_t { NotScheduled, ASAP, WhenReady };

    void timingDidChange(DidSeek, SynchronouslyNotify);
    void updateFinishedState(DidSeek, SynchronouslyNotify);
    void enqueueAnimationPlaybackEvent(const AtomicString&, Optional<Seconds>, Optional<Seconds>);
    Seconds effectEndTime() const;
    WebAnimation& readyPromiseResolve();
    WebAnimation& finishedPromiseResolve();
    Optional<Seconds> currentTime(RespectHoldTime) const;
    ExceptionOr<void> silentlySetCurrentTime(Optional<Seconds>);
    void finishNotificationSteps();
    bool hasPendingPauseTask() const { return m_timeToRunPendingPauseTask != TimeToRunPendingTask::NotScheduled; }
    bool hasPendingPlayTask() const { return m_timeToRunPendingPlayTask != TimeToRunPendingTask::NotScheduled; }
    ExceptionOr<void> play(AutoRewind);
    void runPendingPauseTask();
    void runPendingPlayTask();
    void resetPendingTasks(Silently = Silently::No);
    void setEffectInternal(RefPtr<AnimationEffect>&&, bool = false);
    void setTimelineInternal(RefPtr<AnimationTimeline>&&);
    bool isEffectInvalidationSuspended() { return m_suspendCount; }
    bool computeRelevance();
    void updateRelevance();
    void invalidateEffect();
    double effectivePlaybackRate() const;
    void applyPendingPlaybackRate();

    RefPtr<AnimationEffect> m_effect;
    RefPtr<AnimationTimeline> m_timeline;
    UniqueRef<ReadyPromise> m_readyPromise;
    UniqueRef<FinishedPromise> m_finishedPromise;
    Markable<Seconds, Seconds::MarkableTraits> m_previousCurrentTime;
    Markable<Seconds, Seconds::MarkableTraits> m_startTime;
    Markable<Seconds, Seconds::MarkableTraits> m_holdTime;
    MarkableDouble m_pendingPlaybackRate;
    double m_playbackRate { 1 };
    String m_id;

    int m_suspendCount { 0 };

    bool m_isStopped { false };
    bool m_isSuspended { false };
    bool m_finishNotificationStepsMicrotaskPending;
    bool m_isRelevant;
    bool m_shouldSkipUpdatingFinishedStateWhenResolving;
    TimeToRunPendingTask m_timeToRunPendingPlayTask { TimeToRunPendingTask::NotScheduled };
    TimeToRunPendingTask m_timeToRunPendingPauseTask { TimeToRunPendingTask::NotScheduled };

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return WebAnimationEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::WebAnimation& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
