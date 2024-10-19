/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include "BaseAudioContext.h"
#include "DefaultAudioDestinationNode.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "MediaUniqueIdentifier.h"
#include "PlatformMediaSession.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

class LocalDOMWindow;
class HTMLMediaElement;
class MediaStream;
class MediaStreamAudioDestinationNode;
class MediaStreamAudioSourceNode;

struct AudioContextOptions;
struct AudioTimestamp;

class AudioContext final
    : public BaseAudioContext
    , public MediaProducer
    , public MediaCanStartListener
    , private PlatformMediaSessionClient {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(AudioContext);
public:
    // Create an AudioContext for rendering to the audio hardware.
    static ExceptionOr<Ref<AudioContext>> create(Document&, AudioContextOptions&&);
    ~AudioContext();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::ref(); }

    WEBCORE_EXPORT static void setDefaultSampleRateForTesting(std::optional<float>);

    void close(DOMPromiseDeferred<void>&&);

    DefaultAudioDestinationNode& destination() final { return m_destinationNode.get(); }
    const DefaultAudioDestinationNode& destination() const final { return m_destinationNode.get(); }

    double baseLatency();

    AudioTimestamp getOutputTimestamp();

#if ENABLE(VIDEO)
    ExceptionOr<Ref<MediaElementAudioSourceNode>> createMediaElementSource(HTMLMediaElement&);
#endif
#if ENABLE(MEDIA_STREAM)
    ExceptionOr<Ref<MediaStreamAudioSourceNode>> createMediaStreamSource(MediaStream&);
    ExceptionOr<Ref<MediaStreamAudioDestinationNode>> createMediaStreamDestination();
#endif

    void suspendRendering(DOMPromiseDeferred<void>&&);
    void resumeRendering(DOMPromiseDeferred<void>&&);

    void sourceNodeWillBeginPlayback(AudioNode&) final;
    void lazyInitialize() final;

    void startRendering();

    void isPlayingAudioDidChange();

    // Restrictions to change default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForAudioStartRestriction = 1 << 0,
        RequirePageConsentForAudioStartRestriction = 1 << 1,
    };
    typedef unsigned BehaviorRestrictions;
    BehaviorRestrictions behaviorRestrictions() const { return m_restrictions; }
    void addBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions |= restriction; }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions &= ~restriction; }

    void defaultDestinationWillBecomeConnected();

private:
    AudioContext(Document&, const AudioContextOptions&);

    bool willBeginPlayback();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    uint64_t logIdentifier() const final { return BaseAudioContext::logIdentifier(); }
#endif

    void constructCommon();

    bool userGestureRequiredForAudioStart() const { return m_restrictions & RequireUserGestureForAudioStartRestriction; }
    bool pageConsentRequiredForAudioStart() const { return m_restrictions & RequirePageConsentForAudioStartRestriction; }

    bool willPausePlayback();

    void uninitialize() final;
    bool isOfflineContext() const final { return false; }

    // MediaProducer
    MediaProducerMediaStateFlags mediaState() const final;
    void pageMutedStateDidChange() final;

    // PlatformMediaSessionClient
    PlatformMediaSession::MediaType mediaType() const final { return isSuspended() || isStopped() ? PlatformMediaSession::MediaType::None : PlatformMediaSession::MediaType::WebAudio; }
    PlatformMediaSession::MediaType presentationType() const final { return PlatformMediaSession::MediaType::WebAudio; }
    void mayResumePlayback(bool shouldResume) final;
    void suspendPlayback() final;
    bool canReceiveRemoteControlCommands() const final;
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) final;
    bool supportsSeeking() const final { return false; }
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const final;
    bool canProduceAudio() const final { return true; }
    bool isSuspended() const final;
    bool isPlaying() const final;
    bool isAudible() const final;
    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final;
    bool isNowPlayingEligible() const final;
    std::optional<NowPlayingInfo> nowPlayingInfo() const final;
    WeakPtr<PlatformMediaSession> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSession>>&, PlatformMediaSession::PlaybackControlsPurpose) final;
    void isActiveNowPlayingSessionChanged() final;

    // MediaCanStartListener.
    void mediaCanStart(Document&) final;

    // ActiveDOMObject
    void suspend(ReasonForSuspension) final;
    void resume() final;
    bool virtualHasPendingActivity() const final;

    UniqueRef<DefaultAudioDestinationNode> m_destinationNode;
    std::unique_ptr<PlatformMediaSession> m_mediaSession;
    MediaUniqueIdentifier m_currentIdentifier;

    BehaviorRestrictions m_restrictions { NoRestrictions };

    // [[suspended by user]] flag in the specification:
    // https://www.w3.org/TR/webaudio/#dom-audiocontext-suspended-by-user-slot
    bool m_wasSuspendedByScript { false };

    bool m_canOverrideBackgroundPlaybackRestriction { true };
};

} // WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioContext)
    static bool isType(const WebCore::BaseAudioContext& context) { return !context.isOfflineContext(); }
SPECIALIZE_TYPE_TRAITS_END()
