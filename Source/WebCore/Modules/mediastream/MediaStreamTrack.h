/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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
#include "LongRange.h"
#include "MediaProducer.h"
#include "MediaStreamTrackPrivate.h"
#include "MediaTrackConstraints.h"
#include "PlatformMediaSession.h"
#include <wtf/LoggerHelper.h>

namespace WebCore {

class AudioSourceProvider;
class Document;

struct MediaTrackConstraints;

template<typename IDLType> class DOMPromiseDeferred;

class MediaStreamTrack
    : public RefCounted<MediaStreamTrack>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , private MediaStreamTrackPrivate::Observer
    , private PlatformMediaSession::AudioCaptureSource
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_ISO_ALLOCATED(MediaStreamTrack);
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void trackDidEnd() = 0;
    };

    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);
    virtual ~MediaStreamTrack();

    static void endCapture(Document&, MediaProducerMediaCaptureKind);

    static MediaProducerMediaStateFlags captureState(Document&);
    static void updateCaptureAccordingToMutedState(Document&);

    virtual bool isCanvas() const { return false; }

    const AtomString& kind() const;
    WEBCORE_EXPORT const String& id() const;
    const String& label() const;

    const AtomString& contentHint() const;
    void setContentHint(const String&);
        
    bool enabled() const;
    void setEnabled(bool);

    bool muted() const;
    bool mutedForBindings() const;

    enum class State { Live, Ended };
    State readyState() const { return m_readyState; }

    bool ended() const;

    virtual RefPtr<MediaStreamTrack> clone();

    enum class StopMode { Silently, PostEvent };
    void stopTrack(StopMode = StopMode::Silently);

    bool isCaptureTrack() const { return m_isCaptureTrack; }
    bool hasVideo() const { return m_private->hasVideo(); }
    bool hasAudio() const { return m_private->hasAudio(); }

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

    RefPtr<WebAudioSourceProvider> createAudioSourceProvider();

    MediaProducerMediaStateFlags mediaState() const;

    void addObserver(Observer&);
    void removeObserver(Observer&);

    using RefCounted::ref;
    using RefCounted::deref;

    void setIdForTesting(String&& id) { m_private->setIdForTesting(WTFMove(id)); }

    Document* document() const;

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_private->logger(); }
    const void* logIdentifier() const final { return m_private->logIdentifier(); }
#endif

protected:
    MediaStreamTrack(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);

    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
        
    Ref<MediaStreamTrackPrivate> m_private;
        
private:
    explicit MediaStreamTrack(MediaStreamTrack&);

    void configureTrackRendering();
    void updateToPageMutedState();

    // ActiveDOMObject API.
    void stop() final { stopTrack(); }
    const char* activeDOMObjectName() const override;
    void suspend(ReasonForSuspension) final;
    bool virtualHasPendingActivity() const final;

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

    // PlatformMediaSession::AudioCaptureSource
    bool isCapturingAudio() const final;

#if !RELEASE_LOG_DISABLED
    const char* logClassName() const final { return "MediaStreamTrack"; }
    WTFLogChannel& logChannel() const final;
#endif

    Vector<Observer*> m_observers;

    MediaTrackConstraints m_constraints;
    std::unique_ptr<DOMPromiseDeferred<void>> m_promise;

    State m_readyState { State::Live };
    bool m_muted { false };
    bool m_ended { false };
    const bool m_isCaptureTrack { false };
    bool m_isInterrupted { false };
};

typedef Vector<RefPtr<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
