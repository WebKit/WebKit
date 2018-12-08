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
#include "GenericTaskQueue.h"
#include "JSDOMPromiseDeferred.h"
#include "LongRange.h"
#include "MediaProducer.h"
#include "MediaStreamTrackPrivate.h"
#include "MediaTrackConstraints.h"

namespace WebCore {

class AudioSourceProvider;
class Document;

struct MediaTrackConstraints;

class MediaStreamTrack :
    public RefCounted<MediaStreamTrack>,
    public ActiveDOMObject,
    public EventTargetWithInlineData,
    public CanMakeWeakPtr<MediaStreamTrack>,
    private MediaProducer,
    private MediaStreamTrackPrivate::Observer {
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void trackDidEnd() = 0;
    };

    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);
    virtual ~MediaStreamTrack();

    virtual bool isCanvas() const { return false; }

    const AtomicString& kind() const;
    WEBCORE_EXPORT const String& id() const;
    const String& label() const;


    const AtomicString& contentHint() const;
    void setContentHint(const String&);
        
    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;

    enum class State { Live, Ended };
    State readyState() const;

    bool ended() const;

    virtual RefPtr<MediaStreamTrack> clone();

    enum class StopMode { Silently, PostEvent };
    void stopTrack(StopMode = StopMode::Silently);

    bool isCaptureTrack() const { return m_private->isCaptureTrack(); }

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
        std::optional<bool> displaySurface;
        String logicalSurface;
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
    void applyConstraints(const std::optional<MediaTrackConstraints>&, DOMPromiseDeferred<void>&&);

    RealtimeMediaSource& source() const { return m_private->source(); }
    MediaStreamTrackPrivate& privateTrack() { return m_private.get(); }

    AudioSourceProvider* audioSourceProvider();

    // MediaProducer
    void pageMutedStateDidChange() final;
    MediaProducer::MediaStateFlags mediaState() const final;

    void addObserver(Observer&);
    void removeObserver(Observer&);

    using RefCounted::ref;
    using RefCounted::deref;

    // ActiveDOMObject API.
    bool hasPendingActivity() const final;

    void setIdForTesting(String&& id) { m_private->setIdForTesting(WTFMove(id)); }

protected:
    MediaStreamTrack(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
        
    Ref<MediaStreamTrackPrivate> m_private;
        
private:
    explicit MediaStreamTrack(MediaStreamTrack&);

    void configureTrackRendering();

    Document* document() const;

    // ActiveDOMObject API.
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    EventTargetInterface eventTargetInterface() const final { return MediaStreamTrackEventTargetInterfaceType; }

    // MediaStreamTrackPrivate::Observer
    void trackStarted(MediaStreamTrackPrivate&) final;
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final;
    void trackSettingsChanged(MediaStreamTrackPrivate&) final;
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;

    Vector<Observer*> m_observers;


    MediaTrackConstraints m_constraints;
    std::optional<DOMPromiseDeferred<void>> m_promise;
    GenericTaskQueue<ScriptExecutionContext> m_taskQueue;

    bool m_ended { false };
};

typedef Vector<RefPtr<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
