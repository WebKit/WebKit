/*
 * Copyright (C) 2020-2023 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>
#if PLATFORM(COCOA)

#include <WebCore/AudioHardwareListener.h>
#include <WebCore/AudioSession.h>
#include <WebCore/NowPlayingManager.h>
#include <WebCore/PlatformMediaSessionManager.h>
#include <WebCore/RemoteCommandListener.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct NowPlayingInfo;

enum class MediaPlayerPitchCorrectionAlgorithm : uint8_t;

class WEBCORE_EXPORT MediaSessionManagerCocoa
    : public PlatformMediaSessionManager
    , public NowPlayingManagerClient
    , public AudioHardwareListener::Client {
    WTF_MAKE_TZONE_ALLOCATED(MediaSessionManagerCocoa);
public:
    MediaSessionManagerCocoa(PageIdentifier);
    
    static WEBCORE_EXPORT void clearNowPlayingInfo();
    static WEBCORE_EXPORT void setNowPlayingInfo(bool setAsNowPlayingApplication, bool shouldUpdateNowPlayingSuppression, const NowPlayingInfo&);

    static String audioTimePitchAlgorithmForMediaPlayerPitchCorrectionAlgorithm(MediaPlayerPitchCorrectionAlgorithm, bool preservesPitch, double rate);

protected:
    void updateSessionState() override;
    void beginInterruption(PlatformMediaSession::InterruptionType) final;

    bool hasActiveNowPlayingSession() const final { return m_nowPlayingActive; }
    String lastUpdatedNowPlayingTitle() const final { return m_lastUpdatedNowPlayingTitle; }
    double lastUpdatedNowPlayingDuration() const final { return m_lastUpdatedNowPlayingDuration; }
    double lastUpdatedNowPlayingElapsedTime() const final { return m_lastUpdatedNowPlayingElapsedTime; }
    std::optional<MediaUniqueIdentifier> lastUpdatedNowPlayingInfoUniqueIdentifier() const final { return m_lastUpdatedNowPlayingInfoUniqueIdentifier; }
    bool registeredAsNowPlayingApplication() const final { return m_registeredAsNowPlayingApplication; }
    bool haveEverRegisteredAsNowPlayingApplication() const final { return m_haveEverRegisteredAsNowPlayingApplication; }

    std::optional<NowPlayingInfo> nowPlayingInfo() const final { return m_nowPlayingInfo; }

    void scheduleSessionStatusUpdate() final;
    void updateNowPlayingInfo();
    void updateActiveNowPlayingSession(RefPtr<PlatformMediaSessionInterface>);

    void removeSession(PlatformMediaSessionInterface&) override;
    void addSession(PlatformMediaSessionInterface&) override;
    void setCurrentSession(PlatformMediaSessionInterface&) override;

    bool sessionWillBeginPlayback(PlatformMediaSessionInterface&) override;
    void sessionWillEndPlayback(PlatformMediaSessionInterface&, DelayCallingUpdateNowPlaying) override;
    void sessionDidEndRemoteScrubbing(PlatformMediaSessionInterface&) final;
    void clientCharacteristicsChanged(PlatformMediaSessionInterface&, bool) final;
    void sessionCanProduceAudioChanged() final;

    WeakPtr<PlatformMediaSessionInterface> nowPlayingEligibleSession();

    void addSupportedCommand(PlatformMediaSession::RemoteControlCommandType) final;
    void removeSupportedCommand(PlatformMediaSession::RemoteControlCommandType) final;
    RemoteCommandListener::RemoteCommandsSet supportedCommands() const final;

    void resetHaveEverRegisteredAsNowPlayingApplicationForTesting() final { m_haveEverRegisteredAsNowPlayingApplication = false; };
    void resetSessionState() final;

    RefPtr<AudioHardwareListener> audioHardwareListener() const;

    // AudioHardwareListenerClient
    void audioHardwareDidBecomeActive() override;
    void audioHardwareDidBecomeInactive() override;
    void audioOutputDeviceChanged() override;

private:
    // NowPlayingManagerClient
    void didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType, const PlatformMediaSession::RemoteCommandArgument&) final;

    void possiblyChangeAudioCategory();

    std::optional<bool> supportsSpatialAudioPlaybackForConfiguration(const MediaConfiguration&) final;

#if USE(NOW_PLAYING_ACTIVITY_SUPPRESSION)
    static void updateNowPlayingSuppression(const NowPlayingInfo*);
#endif

#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const override;
#endif

    bool m_nowPlayingActive { false };
    bool m_registeredAsNowPlayingApplication { false };
    bool m_haveEverRegisteredAsNowPlayingApplication { false };

    // For testing purposes only.
    String m_lastUpdatedNowPlayingTitle;
    double m_lastUpdatedNowPlayingDuration { NAN };
    double m_lastUpdatedNowPlayingElapsedTime { NAN };
    Markable<MediaUniqueIdentifier> m_lastUpdatedNowPlayingInfoUniqueIdentifier;
    std::optional<NowPlayingInfo> m_nowPlayingInfo;

    const std::unique_ptr<NowPlayingManager> m_nowPlayingManager;
    RefPtr<AudioHardwareListener> m_audioHardwareListener;

    AudioHardwareListener::BufferSizeRange m_supportedAudioHardwareBufferSizes;
    std::optional<size_t> m_defaultBufferSize;

    RunLoop::Timer m_delayCategoryChangeTimer;
    AudioSession::CategoryType m_previousCategory { AudioSession::CategoryType::None };
    bool m_previousHadAudibleAudioOrVideoMediaType { false };
};

inline RefPtr<AudioHardwareListener> MediaSessionManagerCocoa::audioHardwareListener() const { return m_audioHardwareListener; }

inline void MediaSessionManagerCocoa::audioHardwareDidBecomeActive() { }

inline void MediaSessionManagerCocoa::audioHardwareDidBecomeInactive() { }

inline void MediaSessionManagerCocoa::didReceiveRemoteControlCommand(PlatformMediaSession::RemoteControlCommandType type, const PlatformMediaSession::RemoteCommandArgument& argument) { processDidReceiveRemoteControlCommand(type, argument); }

#if !RELEASE_LOG_DISABLED
inline ASCIILiteral MediaSessionManagerCocoa::logClassName() const { return "MediaSessionManagerCocoa"_s; }
#endif

} // namespace WebCore

#endif // PLATFORM(COCOA)
