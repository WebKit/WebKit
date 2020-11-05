/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#include "AudioContextOptions.h"
#include "BaseAudioContext.h"
#include "DefaultAudioDestinationNode.h"
#include "MediaCanStartListener.h"
#include "MediaProducer.h"
#include "PlatformMediaSession.h"
#include "VisibilityChangeClient.h"

namespace WebCore {

class DOMWindow;

struct AudioTimestamp;

class AudioContext
    : public BaseAudioContext
    , public MediaProducer
    , public MediaCanStartListener
    , private PlatformMediaSessionClient
    , private VisibilityChangeClient {
    WTF_MAKE_ISO_ALLOCATED(AudioContext);
public:
    // Create an AudioContext for rendering to the audio hardware.
    static ExceptionOr<Ref<AudioContext>> create(Document&, AudioContextOptions&& = { });
    ~AudioContext();

    WEBCORE_EXPORT static void setDefaultSampleRateForTesting(Optional<float>);

    void close(DOMPromiseDeferred<void>&&);

    DefaultAudioDestinationNode* destination();
    double baseLatency();

    AudioTimestamp getOutputTimestamp(DOMWindow&);

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

protected:
    explicit AudioContext(Document&, const AudioContextOptions& = { });
    AudioContext(Document&, unsigned numberOfChannels, float sampleRate, RefPtr<AudioBuffer>&& renderTarget);

    bool willBeginPlayback();

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const final;
#endif

private:
    void constructCommon();

    bool userGestureRequiredForAudioStart() const { return !isOfflineContext() && m_restrictions & RequireUserGestureForAudioStartRestriction; }
    bool pageConsentRequiredForAudioStart() const { return !isOfflineContext() && m_restrictions & RequirePageConsentForAudioStartRestriction; }

    bool willPausePlayback();

    // MediaProducer
    MediaProducer::MediaStateFlags mediaState() const override;
    void pageMutedStateDidChange() override;

    // PlatformMediaSessionClient
    PlatformMediaSession::MediaType mediaType() const override { return PlatformMediaSession::MediaType::WebAudio; }
    PlatformMediaSession::MediaType presentationType() const override { return PlatformMediaSession::MediaType::WebAudio; }
    void mayResumePlayback(bool shouldResume) override;
    void suspendPlayback() override;
    bool canReceiveRemoteControlCommands() const override { return false; }
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument*) override { }
    bool supportsSeeking() const override { return false; }
    bool shouldOverrideBackgroundPlaybackRestriction(PlatformMediaSession::InterruptionType) const override { return false; }
    bool canProduceAudio() const final { return true; }
    bool isSuspended() const final;
    DocumentIdentifier hostingDocumentIdentifier() const final;

    // MediaCanStartListener.
    void mediaCanStart(Document&) override;

    // VisibilityChangeClient
    void visibilityStateChanged() final;

    // ActiveDOMObject
    void suspend(ReasonForSuspension) final;
    void resume() final;

    std::unique_ptr<PlatformMediaSession> m_mediaSession;

    BehaviorRestrictions m_restrictions { NoRestrictions };

    // [[suspended by user]] flag in the specification:
    // https://www.w3.org/TR/webaudio/#dom-audiocontext-suspended-by-user-slot
    bool m_wasSuspendedByScript { false };
};

} // WebCore
