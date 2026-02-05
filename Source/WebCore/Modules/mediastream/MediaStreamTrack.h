/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include <WebCore/ActiveDOMObject.h>
#include <WebCore/Blob.h>
#include <WebCore/EventTarget.h>
#include <WebCore/EventTargetInterfaces.h>
#include <WebCore/IDLTypes.h>
#include <WebCore/JSDOMPromiseDeferred.h>
#include <WebCore/MediaProducer.h>
#include <WebCore/MediaStreamTrackDataHolder.h>
#include <WebCore/MediaStreamTrackPrivate.h>
#include <WebCore/MediaTrackCapabilities.h>
#include <WebCore/MediaTrackConstraints.h>
#include <WebCore/PhotoCapabilities.h>
#include <WebCore/PhotoSettings.h>
#include <WebCore/PlatformMediaSession.h>
#include <wtf/LoggerHelper.h>

namespace WebCore {

class AudioSourceProvider;
class Document;
class MediaSessionManagerInterface;

struct MediaTrackConstraints;

class MediaStreamTrack
    : public RefCounted<MediaStreamTrack>
    , public ActiveDOMObject
    , public EventTarget
    , private MediaStreamTrackPrivateObserver
    , private AudioCaptureSource
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(MediaStreamTrack);
public:
    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void trackDidEnd() = 0;
    };

    enum class RegisterCaptureTrackToOwner : bool { No, Yes };
    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&, RegisterCaptureTrackToOwner = RegisterCaptureTrackToOwner::Yes);
    static Ref<MediaStreamTrack> create(ScriptExecutionContext&, UniqueRef<MediaStreamTrackDataHolder>&&);
    virtual ~MediaStreamTrack();

    // ContextDestructionObserver, AudioCaptureSource.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    static MediaProducerMediaStateFlags captureState(const RealtimeMediaSource&);

    virtual bool isCanvas() const { return false; }

    const AtomString& kind() const;
    const String& id() const { return m_private->id(); }
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
    bool isVideo() const { return m_private->isVideo(); }
    bool isAudio() const { return m_private->isAudio(); }

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
        String displaySurface;

        String whiteBalanceMode;
        std::optional<double> zoom;
        std::optional<bool> torch;
        std::optional<bool> backgroundBlur;
        std::optional<bool> powerEfficient;
    };
    TrackSettings getSettings() const;

    using TrackCapabilities = MediaTrackCapabilities;
    TrackCapabilities getCapabilities() const;

    using TakePhotoPromise = NativePromise<std::pair<Vector<uint8_t>, String>, Exception>;
    Ref<TakePhotoPromise> takePhoto(PhotoSettings&&);

    using PhotoCapabilitiesPromise = NativePromise<PhotoCapabilities, Exception>;
    Ref<PhotoCapabilitiesPromise> getPhotoCapabilities();

    using PhotoSettingsPromise = NativePromise<PhotoSettings, Exception>;
    Ref<PhotoSettingsPromise> getPhotoSettings();

    const MediaTrackConstraints& getConstraints() const { return m_constraints; }
    void setConstraints(MediaTrackConstraints&& constraints) { m_constraints = WTF::move(constraints); }

    void applyConstraints(const std::optional<MediaTrackConstraints>&, DOMPromiseDeferred<void>&&);

    RealtimeMediaSource& source() const { return m_private->source(); }
    Ref<RealtimeMediaSource> protectedSource() const { return source(); }
    RealtimeMediaSource& sourceForProcessor() const { return m_private->sourceForProcessor(); }
    MediaStreamTrackPrivate& privateTrack() { return m_private.get(); }
    const MediaStreamTrackPrivate& privateTrack() const { return m_private.get(); }

#if ENABLE(WEB_AUDIO)
    RefPtr<WebAudioSourceProvider> createAudioSourceProvider();
#endif

    MediaProducerMediaStateFlags mediaState() const;

    void addObserver(Observer&);
    void removeObserver(Observer&);

    void setIdForTesting(String&& id) { m_private->setIdForTesting(WTF::move(id)); }

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final { return m_private->logger(); }
    uint64_t logIdentifier() const final { return m_private->logIdentifier(); }
#endif

    void setShouldFireMuteEventImmediately(bool value) { m_shouldFireMuteEventImmediately = value; }

    struct Storage {
        bool enabled { false };
        bool ended { false };
        bool muted { false };
        RealtimeMediaSourceSettings settings;
        RealtimeMediaSourceCapabilities capabilities;
        RefPtr<RealtimeMediaSource> source;
    };

    bool isDetached() const { return m_isDetached; }
    UniqueRef<MediaStreamTrackDataHolder> detach();

    void setMediaStreamId(const String& id) { m_mediaStreamId = id; }
    const String& mediaStreamId() const { return m_mediaStreamId; }

    ScriptExecutionContext* scriptExecutionContext() const final;

    class Keeper : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<Keeper> {
    public:
        static Ref<Keeper> create() { return adoptRef(*new Keeper);}
    private:
        Keeper() = default;
    };

    Ref<Keeper> keeper();

protected:
    MediaStreamTrack(ScriptExecutionContext&, Ref<MediaStreamTrackPrivate>&&);

    using ActiveDOMObject::protectedScriptExecutionContext;

private:
    explicit MediaStreamTrack(MediaStreamTrack&);

    void configureTrackRendering();

    // ActiveDOMObject.
    void stop() final { stopTrack(); }
    void suspend(ReasonForSuspension) final;
    bool virtualHasPendingActivity() const final;

    // EventTarget
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    enum EventTargetInterfaceType eventTargetInterface() const final { return EventTargetInterfaceType::MediaStreamTrack; }

    // MediaStreamTrackPrivateObserver
    void trackStarted(MediaStreamTrackPrivate&) final;
    void trackEnded(MediaStreamTrackPrivate&) final;
    void trackMutedChanged(MediaStreamTrackPrivate&) final;
    void trackSettingsChanged(MediaStreamTrackPrivate&) final;
    void trackEnabledChanged(MediaStreamTrackPrivate&) final;
    void trackConfigurationChanged(MediaStreamTrackPrivate&) final;

    // AudioCaptureSource
    bool isCapturingAudio() const final;
    bool wantsToCaptureAudio() const final;

    RefPtr<MediaSessionManagerInterface> mediaSessionManager() const;

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "MediaStreamTrack"_s; }
    WTFLogChannel& logChannel() const final;
#endif

    Vector<Observer*> m_observers;

    MediaTrackConstraints m_constraints;

    const Ref<MediaStreamTrackPrivate> m_private;
    String m_mediaStreamId;
    State m_readyState { State::Live };
    bool m_muted { false };
    bool m_ended { false };
    const bool m_isCaptureTrack { false };
    bool m_isInterrupted { false };
    bool m_shouldFireMuteEventImmediately { false };
    bool m_isDetached { false };
    mutable AtomString m_kind;
    mutable AtomString m_contentHint;
    ThreadSafeWeakPtr<Keeper> m_keeper;
};

typedef Vector<Ref<MediaStreamTrack>> MediaStreamTrackVector;

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_EVENTTARGET(MediaStreamTrack)

#endif // ENABLE(MEDIA_STREAM)
