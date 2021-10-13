/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>
#include <wtf/OptionSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

enum class MediaProducerMediaState : uint32_t {
    IsPlayingAudio = 1 << 0,
    IsPlayingVideo = 1 << 1,
    IsPlayingToExternalDevice = 1 << 2,
    RequiresPlaybackTargetMonitoring = 1 << 3,
    ExternalDeviceAutoPlayCandidate = 1 << 4,
    DidPlayToEnd = 1 << 5,
    IsSourceElementPlaying = 1 << 6,
    IsNextTrackControlEnabled = 1 << 7,
    IsPreviousTrackControlEnabled = 1 << 8,
    HasPlaybackTargetAvailabilityListener = 1 << 9,
    HasAudioOrVideo = 1 << 10,
    HasActiveAudioCaptureDevice = 1 << 11,
    HasActiveVideoCaptureDevice = 1 << 12,
    HasMutedAudioCaptureDevice = 1 << 13,
    HasMutedVideoCaptureDevice = 1 << 14,
    HasInterruptedAudioCaptureDevice = 1 << 15,
    HasInterruptedVideoCaptureDevice = 1 << 16,
    HasUserInteractedWithMediaElement = 1 << 17,
    HasActiveDisplayCaptureDevice = 1 << 18,
    HasMutedDisplayCaptureDevice = 1 << 19,
    HasInterruptedDisplayCaptureDevice = 1 << 20,
};
using MediaProducerMediaStateFlags = OptionSet<MediaProducerMediaState>;

enum class MediaProducerMediaCaptureKind : uint8_t {
    Audio,
    Video,
    AudioVideo
};

enum class MediaProducerMutedState : uint8_t {
    AudioIsMuted = 1 << 0,
    AudioCaptureIsMuted = 1 << 1,
    VideoCaptureIsMuted = 1 << 2,
    ScreenCaptureIsMuted = 1 << 3,
};
using MediaProducerMutedStateFlags = OptionSet<MediaProducerMutedState>;

class MediaProducer : public CanMakeWeakPtr<MediaProducer> {
public:
    using MediaState = MediaProducerMediaState;
    using MutedState = MediaProducerMutedState;
    using MediaStateFlags = MediaProducerMediaStateFlags;
    using MutedStateFlags = MediaProducerMutedStateFlags;

    static constexpr MediaStateFlags IsNotPlaying = { };
    static constexpr MediaStateFlags AudioCaptureMask = { MediaState::HasActiveAudioCaptureDevice, MediaState::HasMutedAudioCaptureDevice, MediaState::HasInterruptedAudioCaptureDevice };
    static constexpr MediaStateFlags VideoCaptureMask = { MediaState::HasActiveVideoCaptureDevice, MediaState::HasMutedVideoCaptureDevice, MediaState::HasInterruptedVideoCaptureDevice };
    static constexpr MediaStateFlags DisplayCaptureMask = { MediaState::HasActiveDisplayCaptureDevice, MediaState::HasMutedDisplayCaptureDevice, MediaState::HasInterruptedDisplayCaptureDevice };
    static constexpr MediaStateFlags ActiveCaptureMask = { MediaState::HasActiveAudioCaptureDevice, MediaState::HasActiveVideoCaptureDevice, MediaState::HasActiveDisplayCaptureDevice };
    static constexpr MediaStateFlags MutedCaptureMask = { MediaState::HasMutedAudioCaptureDevice, MediaState::HasMutedVideoCaptureDevice, MediaState::HasMutedDisplayCaptureDevice };
    static constexpr MediaStateFlags MediaCaptureMask = { MediaState::HasActiveAudioCaptureDevice, MediaState::HasMutedAudioCaptureDevice, MediaState::HasInterruptedAudioCaptureDevice, MediaState::HasActiveVideoCaptureDevice, MediaState::HasMutedVideoCaptureDevice, MediaState::HasInterruptedVideoCaptureDevice, MediaState::HasActiveDisplayCaptureDevice, MediaState::HasMutedDisplayCaptureDevice, MediaState::HasInterruptedDisplayCaptureDevice };

    static bool isCapturing(MediaStateFlags state) { return state.containsAny(ActiveCaptureMask) || state.containsAny(MutedCaptureMask); }

    virtual MediaStateFlags mediaState() const = 0;

    static constexpr MutedStateFlags AudioAndVideoCaptureIsMuted = { MutedState::AudioCaptureIsMuted, MutedState::VideoCaptureIsMuted };
    static constexpr MutedStateFlags MediaStreamCaptureIsMuted = { MutedState::AudioCaptureIsMuted, MutedState::VideoCaptureIsMuted, MutedState::ScreenCaptureIsMuted };

    virtual void pageMutedStateDidChange() = 0;

protected:
    virtual ~MediaProducer() = default;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaProducerMediaCaptureKind> {
    using values = EnumValues<
        WebCore::MediaProducerMediaCaptureKind,
        WebCore::MediaProducerMediaCaptureKind::Audio,
        WebCore::MediaProducerMediaCaptureKind::Video,
        WebCore::MediaProducerMediaCaptureKind::AudioVideo
    >;
};

template<> struct EnumTraits<WebCore::MediaProducerMediaState> {
    using values = EnumValues<
        WebCore::MediaProducerMediaState,
        WebCore::MediaProducerMediaState::IsPlayingAudio,
        WebCore::MediaProducerMediaState::IsPlayingVideo,
        WebCore::MediaProducerMediaState::IsPlayingToExternalDevice,
        WebCore::MediaProducerMediaState::RequiresPlaybackTargetMonitoring,
        WebCore::MediaProducerMediaState::ExternalDeviceAutoPlayCandidate,
        WebCore::MediaProducerMediaState::DidPlayToEnd,
        WebCore::MediaProducerMediaState::IsSourceElementPlaying,
        WebCore::MediaProducerMediaState::IsNextTrackControlEnabled,
        WebCore::MediaProducerMediaState::IsPreviousTrackControlEnabled,
        WebCore::MediaProducerMediaState::HasPlaybackTargetAvailabilityListener,
        WebCore::MediaProducerMediaState::HasAudioOrVideo,
        WebCore::MediaProducerMediaState::HasActiveAudioCaptureDevice,
        WebCore::MediaProducerMediaState::HasActiveVideoCaptureDevice,
        WebCore::MediaProducerMediaState::HasMutedAudioCaptureDevice,
        WebCore::MediaProducerMediaState::HasMutedVideoCaptureDevice,
        WebCore::MediaProducerMediaState::HasInterruptedAudioCaptureDevice,
        WebCore::MediaProducerMediaState::HasInterruptedVideoCaptureDevice,
        WebCore::MediaProducerMediaState::HasUserInteractedWithMediaElement,
        WebCore::MediaProducerMediaState::HasActiveDisplayCaptureDevice,
        WebCore::MediaProducerMediaState::HasMutedDisplayCaptureDevice,
        WebCore::MediaProducerMediaState::HasInterruptedDisplayCaptureDevice
    >;
};

template<> struct EnumTraits<WebCore::MediaProducerMutedState> {
    using values = EnumValues<
        WebCore::MediaProducerMutedState,
        WebCore::MediaProducerMutedState::AudioIsMuted,
        WebCore::MediaProducerMutedState::AudioCaptureIsMuted,
        WebCore::MediaProducerMutedState::VideoCaptureIsMuted,
        WebCore::MediaProducerMutedState::ScreenCaptureIsMuted
    >;
};

}
