/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebCore {
class AudioCaptureSource;
}

namespace WTF {
template<typename T> struct IsDeprecatedWeakRefSmartPointerException;
template<> struct IsDeprecatedWeakRefSmartPointerException<WebCore::AudioCaptureSource> : std::true_type { };
}

namespace WebCore {

enum class AudioSessionCategory : uint8_t;
enum class AudioSessionMode : uint8_t;
enum class RouteSharingPolicy : uint8_t;

enum class PlatformMediaSessionMediaType : uint8_t {
    None,
    Video,
    VideoAudio,
    Audio,
    WebAudio,
};

enum class PlatformMediaSessionState : uint8_t {
    Idle,
    Autoplaying,
    Playing,
    Paused,
    Interrupted,
};

enum class PlatformMediaSessionInterruptionType : uint8_t {
    NoInterruption,
    SystemSleep,
    EnteringBackground,
    SystemInterruption,
    SuspendedUnderLock,
    InvisibleAutoplay,
    ProcessInactive,
    PlaybackSuspended,
    PageNotVisible,
};

enum class PlatformMediaSessionPlaybackControlsPurpose : uint8_t {
    ControlsManager,
    NowPlaying,
    MediaSession
};

enum class PlatformMediaSessionDisplayType : uint8_t {
    Normal,
    Fullscreen,
    Optimized,
};

enum class PlatformMediaSessionEndInterruptionFlags : uint8_t {
    NoFlags = 0,
    MayResumePlaying = 1 << 0,
};

enum class DelayCallingUpdateNowPlaying : bool {
    No,
    Yes
};

enum class PlatformMediaSessionRemoteControlCommandType : uint8_t {
    NoCommand,
    PlayCommand,
    PauseCommand,
    StopCommand,
    TogglePlayPauseCommand,
    BeginSeekingBackwardCommand,
    EndSeekingBackwardCommand,
    BeginSeekingForwardCommand,
    EndSeekingForwardCommand,
    SeekToPlaybackPositionCommand,
    SkipForwardCommand,
    SkipBackwardCommand,
    NextTrackCommand,
    PreviousTrackCommand,
    BeginScrubbingCommand,
    EndScrubbingCommand,
};

struct PlatformMediaSessionRemoteCommandArgument {
    std::optional<double> time;
    std::optional<bool> fastSeek;
};

using PlatformMediaSessionRemoteCommandsSet = HashSet<PlatformMediaSessionRemoteControlCommandType, IntHash<PlatformMediaSessionRemoteControlCommandType>, WTF::StrongEnumHashTraits<PlatformMediaSessionRemoteControlCommandType>>;

class AudioCaptureSource : public CanMakeWeakPtr<AudioCaptureSource> {
public:
    virtual ~AudioCaptureSource() = default;
    virtual bool isCapturingAudio() const = 0;
    virtual bool wantsToCaptureAudio() const = 0;
};

} // namespace WebCore
