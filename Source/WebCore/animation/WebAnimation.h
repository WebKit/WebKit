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
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "WebAnimationTypes.h"
#include <wtf/Forward.h>
#include <wtf/Markable.h>
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/Seconds.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AnimationEffect;
class AnimationEventBase;
class AnimationTimeline;
class Document;
class Element;
class RenderStyle;

template<typename IDLType> class DOMPromiseProxyWithResolveCallback;

class WebAnimation : public RefCounted<WebAnimation>, public CanMakeWeakPtr<WebAnimation>, public EventTargetWithInlineData, public ActiveDOMObject {
    WTF_MAKE_ISO_ALLOCATED(WebAnimation);
public:
    static Ref<WebAnimation> create(Document&, AnimationEffect*);
    static Ref<WebAnimation> create(Document&, AnimationEffect*, AnimationTimeline*);
    ~WebAnimation();

    WEBCORE_EXPORT static HashSet<WebAnimation*>& instances();

    virtual bool isDeclarativeAnimation() const { return false; }
    virtual bool isCSSAnimation() const { return false; }
    virtual bool isCSSTransition() const { return false; }

    const String& id() const { return m_id; }
    void setId(const String&);

    AnimationEffect* bindingsEffect() const { return effect(); }
    virtual void setBindingsEffect(RefPtr<AnimationEffect>&&);
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

    enum class ReplaceState : uint8_t { Active, Removed, Persisted };
    ReplaceState replaceState() const { return m_replaceState; }
    void setReplaceState(ReplaceState replaceState) { m_replaceState = replaceState; }

    bool pending() const { return hasPendingPauseTask() || hasPendingPlayTask(); }

    using ReadyPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<WebAnimation>>;
    ReadyPromise& ready() { return m_readyPromise.get(); }

    using FinishedPromise = DOMPromiseProxyWithResolveCallback<IDLInterface<WebAnimation>>;
    FinishedPromise& finished() { return m_finishedPromise.get(); }

    virtual void cancel(Silently = Silently::No);
    ExceptionOr<void> finish();
    ExceptionOr<void> play();
    void updatePlaybackRate(double);
    ExceptionOr<void> pause();
    virtual ExceptionOr<void> bindingsReverse();
    ExceptionOr<void> reverse();
    void persist();
    ExceptionOr<void> commitStyles();

    virtual Optional<double> bindingsStartTime() const { return startTime(); }
    virtual void setBindingsStartTime(Optional<double>);
    Optional<double> startTime() const;
    void setStartTime(Optional<double>);
    virtual Optional<double> bindingsCurrentTime() const;
    virtual ExceptionOr<void> setBindingsCurrentTime(Optional<double>);
    virtual PlayState bindingsPlayState() const { return playState(); }
    virtual ReplaceState bindingsReplaceState() const { return replaceState(); }
    virtual bool bindingsPending() const { return pending(); }
    virtual ReadyPromise& bindingsReady() { return ready(); }
    virtual FinishedPromise& bindingsFinished() { return finished(); }
    virtual ExceptionOr<void> bindingsPlay() { return play(); }
    virtual ExceptionOr<void> bindingsPause() { return pause(); }

    bool needsTick() const;
    virtual void tick();
    WEBCORE_EXPORT Seconds timeToNextTick() const;
    virtual void resolve(RenderStyle&);
    void effectTargetDidChange(Element* previousTarget, Element* newTarget);
    void acceleratedStateDidChange();
    void applyPendingAcceleratedActions();
    void willChangeRenderer();

    bool isRunningAccelerated() const;
    bool isCompletelyAccelerated() const;
    bool isRelevant() const { return m_isRelevant; }
    void updateRelevance();
    void effectTimingDidChange();
    void suspendEffectInvalidation();
    void unsuspendEffectInvalidation();
    void setSuspended(bool);
    bool isSuspended() const { return m_isSuspended; }
    bool isReplaceable() const;
    void remove();
    void enqueueAnimationPlaybackEvent(const AtomString&, Optional<Seconds>, Optional<Seconds>);

    uint64_t globalPosition() const { return m_globalPosition; }
    void setGlobalPosition(uint64_t globalPosition) { m_globalPosition = globalPosition; }

    virtual bool canHaveGlobalPosition() { return true; }

    // ContextDestructionObserver.
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void contextDestroyed() final;

    using RefCounted::ref;
    using RefCounted::deref;

protected:
    explicit WebAnimation(Document&);

    void enqueueAnimationEvent(Ref<AnimationEventBase>&&);

private:
    enum class DidSeek : uint8_t { Yes, No };
    enum class SynchronouslyNotify : uint8_t { Yes, No };
    enum class RespectHoldTime : uint8_t { Yes, No };
    enum class AutoRewind : uint8_t { Yes, No };
    enum class TimeToRunPendingTask : uint8_t { NotScheduled, ASAP, WhenReady };

    void timingDidChange(DidSeek, SynchronouslyNotify, Silently = Silently::No);
    void updateFinishedState(DidSeek, SynchronouslyNotify);
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

    bool m_isSuspended { false };
    bool m_finishNotificationStepsMicrotaskPending;
    bool m_isRelevant;
    bool m_shouldSkipUpdatingFinishedStateWhenResolving;
    bool m_hasScheduledEventsDuringTick { false };
    TimeToRunPendingTask m_timeToRunPendingPlayTask { TimeToRunPendingTask::NotScheduled };
    TimeToRunPendingTask m_timeToRunPendingPauseTask { TimeToRunPendingTask::NotScheduled };
    ReplaceState m_replaceState { ReplaceState::Active };
    uint64_t m_globalPosition { 0 };

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    void suspend(ReasonForSuspension) final;
    void resume() final;
    void stop() final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return WebAnimationEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_WEB_ANIMATION(ToValueTypeName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
static bool isType(const WebCore::WebAnimation& value) { return value.predicate; } \
SPECIALIZE_TYPE_TRAITS_END()
