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

#include "ActiveDOMObject.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Seconds.h>

namespace WebCore {

class AnimationEffect;
class AnimationPlaybackEvent;
class AnimationTimeline;
class Document;
class RenderStyle;

class WebAnimation final : public RefCounted<WebAnimation>, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    static Ref<WebAnimation> create(Document&, AnimationEffect*, AnimationTimeline*);
    ~WebAnimation();

    AnimationEffect* effect() const { return m_effect.get(); }
    void setEffect(RefPtr<AnimationEffect>&&);
    AnimationTimeline* timeline() const { return m_timeline.get(); }
    void setTimeline(RefPtr<AnimationTimeline>&&);

    std::optional<double> bindingsStartTime() const;
    void setBindingsStartTime(std::optional<double>);
    std::optional<Seconds> startTime() const;
    void setStartTime(std::optional<Seconds>);

    std::optional<double> bindingsCurrentTime() const;
    ExceptionOr<void> setBindingsCurrentTime(std::optional<double>);
    std::optional<Seconds> currentTime() const;
    void setCurrentTime(std::optional<Seconds>);

    double playbackRate() const { return m_playbackRate; }
    void setPlaybackRate(double);

    enum class PlayState { Idle, Pending, Running, Paused, Finished };
    PlayState playState() const;

    // FIXME: return a live value once we support pending pause and play tasks.
    bool pending() const { return false; }

    Seconds timeToNextRequiredTick(Seconds) const;
    void resolve(RenderStyle&);
    void acceleratedRunningStateDidChange();
    void startOrStopAccelerated();

    String description();

    using RefCounted::ref;
    using RefCounted::deref;

private:
    explicit WebAnimation(Document&);

    void enqueueAnimationPlaybackEvent(const AtomicString&, std::optional<Seconds>, std::optional<Seconds>);
    Seconds effectEndTime() const;
    
    RefPtr<AnimationEffect> m_effect;
    RefPtr<AnimationTimeline> m_timeline;
    std::optional<Seconds> m_startTime;
    double m_playbackRate { 1 };
    bool m_isStopped { false };

    // ActiveDOMObject.
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;
    void stop() final;

    // EventTarget
    EventTargetInterface eventTargetInterface() const final { return WebAnimationEventTargetInterfaceType; }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
};

} // namespace WebCore
