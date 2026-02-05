/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
    WTF_MAKE_TZONE_ALLOCATED(AudioContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AudioContext);
public:
    USING_CAN_MAKE_WEAKPTR(EventTarget);

    // Create an AudioContext for rendering to the audio hardware.
    static ExceptionOr<Ref<AudioContext>> create(Document&, AudioContextOptions&&);
    virtual ~AudioContext();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    WEBCORE_EXPORT static void setDefaultSampleRateForTesting(std::optional<float>);

    void close(DOMPromiseDeferred<void>&&);

    DefaultAudioDestinationNode& destination() final { return m_destinationNode.get(); }
    const DefaultAudioDestinationNode& destination() const final { return m_destinationNode.get(); }

    double baseLatency();
    double outputLatency();

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
    enum class BehaviorRestrictionFlags : uint8_t {
        RequireUserGestureForAudioStartRestriction = 1 << 0,
        RequirePageConsentForAudioStartRestriction = 1 << 1,
    };
    using BehaviorRestrictions = OptionSet<BehaviorRestrictionFlags>;
    BehaviorRestrictions behaviorRestrictions() const { return m_restrictions; }
    void addBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions.add(restriction); }
    void removeBehaviorRestriction(BehaviorRestrictions restriction) { m_restrictions.remove(restriction); }

    void defaultDestinationWillBecomeConnected();

#if PLATFORM(IOS_FAMILY)
    const String& sceneIdentifier() const final;
#endif

private:
    AudioContext(Document&, const AudioContextOptions&);

    void willBeginPlayback(CompletionHandler<void(bool)>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
    uint64_t logIdentifier() const final { return BaseAudioContext::logIdentifier(); }
#endif

    void constructCommon();

    bool userGestureRequiredForAudioStart() const { return m_restrictions.contains(BehaviorRestrictionFlags::RequireUserGestureForAudioStartRestriction); }
    bool pageConsentRequiredForAudioStart() const { return m_restrictions.contains(BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction); }

    bool willPausePlayback();

    void uninitialize() final;
    bool isOfflineContext() const final { return false; }

    // MediaProducer
    MediaProducerMediaStateFlags mediaState() const final;
    void pageMutedStateDidChange() final;
#if PLATFORM(IOS_FAMILY)
    void sceneIdentifierDidChange() final;
#endif

    // PlatformMediaSessionClient
    RefPtr<MediaSessionManagerInterface> sessionManager() const final;
    PlatformMediaSession::MediaType mediaType() const final { return isSuspended() || isStopped() ? PlatformMediaSession::MediaType::None : PlatformMediaSession::MediaType::WebAudio; }
    PlatformMediaSession::MediaType presentationType() const final { return PlatformMediaSession::MediaType::WebAudio; }
    void mayResumePlayback(bool shouldResume) final;
    void suspendPlayback() final;
    bool canReceiveRemoteControlCommands() const final;
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) final;
    bool supportsSeeking() const final { return false; }
    bool canProduceAudio() const final { return true; }
    std::optional<MediaSessionGroupIdentifier> mediaSessionGroupIdentifier() const final;
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const final;
    bool isSuspended() const final;
    bool isPlaying() const final;
    bool isAudible() const final;
    bool isNowPlayingEligible() const final;
    std::optional<NowPlayingInfo> nowPlayingInfo() const final;
    WeakPtr<PlatformMediaSessionInterface> selectBestMediaSession(const Vector<WeakPtr<PlatformMediaSessionInterface>>&, PlatformMediaSession::PlaybackControlsPurpose) final;

    // MediaCanStartListener.
    void mediaCanStart(Document&) final;

    // ActiveDOMObject
    void suspend(ReasonForSuspension) final;
    void resume() final;
    bool virtualHasPendingActivity() const final;

    const UniqueRef<DefaultAudioDestinationNode> m_destinationNode;
    const Ref<PlatformMediaSession> m_mediaSession;
    MediaUniqueIdentifier m_currentIdentifier;

    BehaviorRestrictions m_restrictions;

    // [[suspended by user]] flag in the specification:
    // https://www.w3.org/TR/webaudio/#dom-audiocontext-suspended-by-user-slot
    bool m_wasSuspendedByScript { false };

    bool m_canOverrideBackgroundPlaybackRestriction { true };
};

} // WebCore

namespace WTF {
template<> struct EnumTraits<WebCore::AudioContext::BehaviorRestrictionFlags> {
    using values = EnumValues<
        WebCore::AudioContext::BehaviorRestrictionFlags,
        WebCore::AudioContext::BehaviorRestrictionFlags::RequireUserGestureForAudioStartRestriction,
        WebCore::AudioContext::BehaviorRestrictionFlags::RequirePageConsentForAudioStartRestriction
    >;
};
} // namespace WTF

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::AudioContext)
    static bool isType(const WebCore::BaseAudioContext& context) { return !context.isOfflineContext(); }
SPECIALIZE_TYPE_TRAITS_END()
