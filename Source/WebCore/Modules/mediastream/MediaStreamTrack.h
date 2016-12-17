/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "ActiveDOMObject.h"
#include "DoubleRange.h"
#include "EventTarget.h"
#include "JSDOMPromise.h"
#include "LongRange.h"
#include "MediaStreamTrackPrivate.h"
#include "MediaTrackConstraints.h"

namespace WebCore {

class AudioSourceProvider;

struct MediaTrackConstraints;

class MediaStreamTrack final : public RefCounted<MediaStreamTrack>, public ActiveDOMObject, public EventTargetWithInlineData, private MediaStreamTrackPrivate::Observer {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        virtual void trackDidEnd() = 0;
    };

    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);
    virtual ~MediaStreamTrack();

    const AtomicString& kind() const;
    const String& id() const;
    const String& label() const;

    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;
    bool readonly() const;
    bool remote() const;

    enum class State { New, Live, Ended };
    State readyState() const;

    bool ended() const;

    Ref<MediaStreamTrack> clone();
    void stopProducingData();

    struct TrackSettings {
        std::optional<int> width;
        std::optional<int> height;
        std::optional<double> aspectRatio;
        std::optional<double> frameRate;
        String facingMode;
        std::optional<double> volume;
        std::optional<int> sampleRate;
        std::optional<int> sampleSize;
        std::optional<bool> echoCancellation;
        String deviceId;
        String groupId;
    };
    TrackSettings getSettings() const;

    struct TrackCapabilities {
        std::optional<LongRange> width;
        std::optional<LongRange> height;
        std::optional<DoubleRange> aspectRatio;
        std::optional<DoubleRange> frameRate;
        std::optional<Vector<String>> facingMode;
        std::optional<DoubleRange> volume;
        std::optional<LongRange> sampleRate;
        std::optional<LongRange> sampleSize;
        std::optional<Vector<bool>> echoCancellation;
        String deviceId;
        String groupId;
    };
    TrackCapabilities getCapabilities() const;

    const MediaTrackConstraints& getConstraints() const { return m_constraints; }
    void applyConstraints(const std::optional<MediaTrackConstraints>&, DOMPromise<void>&&);

    RealtimeMediaSource& source() { return m_private->source(); }
    MediaStreamTrackPrivate& privateTrack() { return m_private.get(); }

    AudioSourceProvider* audioSourceProvider();

    void addObserver(Observer&);
    void removeObserver(Observer&);

    using RefCounted::ref;
    using RefCounted::deref;

private:
    MediaStreamTrack(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);
    explicit MediaStreamTrack(MediaStreamTrack&);

    void configureTrackRendering();

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return MediaStreamTrackEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }

    // MediaStreamTrackPrivate::Observer
    void trackEnded(MediaStreamTrackPrivate&) override;
    void trackMutedChanged(MediaStreamTrackPrivate&) override;
    void trackSettingsChanged(MediaStreamTrackPrivate&) override;
    void trackEnabledChanged(MediaStreamTrackPrivate&) override;

    WeakPtr<MediaStreamTrack> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    Vector<Observer*> m_observers;
    Ref<MediaStreamTrackPrivate> m_private;

    MediaTrackConstraints m_constraints;
    std::optional<DOMPromise<void>> m_promise;
    WeakPtrFactory<MediaStreamTrack> m_weakPtrFactory;

    bool m_ended { false };
};

typedef Vector<RefPtr<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
