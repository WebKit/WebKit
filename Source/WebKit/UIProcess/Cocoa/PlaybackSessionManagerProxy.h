/*
 * Copyright (C) 2016-2020 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include "PlaybackSessionContextIdentifier.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/PlatformView.h>
#include <WebCore/PlaybackSessionModel.h>
#include <WebCore/TimeRanges.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if PLATFORM(IOS_FAMILY)
#include <WebCore/PlaybackSessionInterfaceAVKit.h>
#else
#include <WebCore/PlaybackSessionInterfaceMac.h>
#endif

#if PLATFORM(IOS_FAMILY)
typedef WebCore::PlaybackSessionInterfaceAVKit PlatformPlaybackSessionInterface;
#else
typedef WebCore::PlaybackSessionInterfaceMac PlatformPlaybackSessionInterface;
#endif

namespace WebKit {

class WebPageProxy;
class PlaybackSessionManagerProxy;

class PlaybackSessionModelContext final: public RefCounted<PlaybackSessionModelContext>, public WebCore::PlaybackSessionModel  {
public:
    static Ref<PlaybackSessionModelContext> create(PlaybackSessionManagerProxy& manager, PlaybackSessionContextIdentifier contextId)
    {
        return adoptRef(*new PlaybackSessionModelContext(manager, contextId));
    }
    virtual ~PlaybackSessionModelContext() { }

    void invalidate() { m_manager = nullptr; }

    // PlaybackSessionModel
    void addClient(WebCore::PlaybackSessionModelClient&) final;
    void removeClient(WebCore::PlaybackSessionModelClient&)final;

    void durationChanged(double);
    void currentTimeChanged(double);
    void bufferedTimeChanged(double);
    void playbackStartedTimeChanged(double);
    void rateChanged(bool isPlaying, float playbackRate);
    void seekableRangesChanged(WebCore::TimeRanges&, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(bool);
    void audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t index);
    void legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t index);
    void audioMediaSelectionIndexChanged(uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(uint64_t selectedIndex);
    void externalPlaybackChanged(bool, PlaybackSessionModel::ExternalPlaybackTargetType, const String&);
    void wirelessVideoPlaybackDisabledChanged(bool);
    void mutedChanged(bool);
    void volumeChanged(double);
    void pictureInPictureSupportedChanged(bool);
    void pictureInPictureActiveChanged(bool);

    bool wirelessVideoPlaybackDisabled() const final { return m_wirelessVideoPlaybackDisabled; }

private:
    friend class VideoFullscreenModelContext;

    PlaybackSessionModelContext(PlaybackSessionManagerProxy& manager, PlaybackSessionContextIdentifier contextId)
        : m_manager(&manager)
        , m_contextId(contextId)
    {
    }

    // PlaybackSessionModel
    void play() final;
    void pause() final;
    void togglePlayState() final;
    void beginScrubbing() final;
    void endScrubbing() final;
    void seekToTime(double, double, double) final;
    void fastSeek(double time) final;
    void beginScanningForward() final;
    void beginScanningBackward() final;
    void endScanning() final;
    void selectAudioMediaOption(uint64_t) final;
    void selectLegibleMediaOption(uint64_t) final;
    void togglePictureInPicture() final;
    void toggleMuted() final;
    void setMuted(bool) final;
    void setVolume(double) final;
    void setPlayingOnSecondScreen(bool) final;

    double playbackStartedTime() const final { return m_playbackStartedTime; }
    double duration() const final { return m_duration; }
    double currentTime() const final { return m_currentTime; }
    double bufferedTime() const final { return m_bufferedTime; }
    bool isPlaying() const final { return m_isPlaying; }
    bool isScrubbing() const final { return m_isScrubbing; }
    float playbackRate() const final { return m_playbackRate; }
    Ref<WebCore::TimeRanges> seekableRanges() const final { return m_seekableRanges.copyRef(); }
    double seekableTimeRangesLastModifiedTime() const final { return m_seekableTimeRangesLastModifiedTime; }
    double liveUpdateInterval() const { return m_liveUpdateInterval; }
    bool canPlayFastReverse() const final { return m_canPlayFastReverse; }
    Vector<WebCore::MediaSelectionOption> audioMediaSelectionOptions() const final { return m_audioMediaSelectionOptions; }
    uint64_t audioMediaSelectedIndex() const final { return m_audioMediaSelectedIndex; }
    Vector<WebCore::MediaSelectionOption> legibleMediaSelectionOptions() const final { return m_legibleMediaSelectionOptions; }
    uint64_t legibleMediaSelectedIndex() const final { return m_legibleMediaSelectedIndex; }
    bool externalPlaybackEnabled() const final { return m_externalPlaybackEnabled; }
    PlaybackSessionModel::ExternalPlaybackTargetType externalPlaybackTargetType() const final { return m_externalPlaybackTargetType; }
    String externalPlaybackLocalizedDeviceName() const final { return m_externalPlaybackLocalizedDeviceName; }
    bool isMuted() const final { return m_muted; }
    double volume() const final { return m_volume; }
    bool isPictureInPictureSupported() const final { return m_pictureInPictureSupported; }
    bool isPictureInPictureActive() const final { return m_pictureInPictureActive; }

    PlaybackSessionManagerProxy* m_manager;
    PlaybackSessionContextIdentifier m_contextId;
    HashSet<WebCore::PlaybackSessionModelClient*> m_clients;
    double m_playbackStartedTime { 0 };
    bool m_playbackStartedTimeNeedsUpdate { false };
    double m_duration { 0 };
    double m_currentTime { 0 };
    double m_bufferedTime { 0 };
    bool m_isPlaying { false };
    bool m_isScrubbing { false };
    float m_playbackRate { 0 };
    Ref<WebCore::TimeRanges> m_seekableRanges { WebCore::TimeRanges::create() };
    double m_seekableTimeRangesLastModifiedTime { 0 };
    double m_liveUpdateInterval { 0 };
    bool m_canPlayFastReverse { false };
    Vector<WebCore::MediaSelectionOption> m_audioMediaSelectionOptions;
    uint64_t m_audioMediaSelectedIndex { 0 };
    Vector<WebCore::MediaSelectionOption> m_legibleMediaSelectionOptions;
    uint64_t m_legibleMediaSelectedIndex { 0 };
    bool m_externalPlaybackEnabled { false };
    PlaybackSessionModel::ExternalPlaybackTargetType m_externalPlaybackTargetType { PlaybackSessionModel::TargetTypeNone };
    String m_externalPlaybackLocalizedDeviceName;
    bool m_wirelessVideoPlaybackDisabled { false };
    bool m_muted { false };
    double m_volume { 0 };
    bool m_pictureInPictureSupported { false };
    bool m_pictureInPictureActive { false };
};

class PlaybackSessionManagerProxy : public RefCounted<PlaybackSessionManagerProxy>, private IPC::MessageReceiver {
public:
    static Ref<PlaybackSessionManagerProxy> create(WebPageProxy&);
    virtual ~PlaybackSessionManagerProxy();

    void invalidate();

    PlatformPlaybackSessionInterface* controlsManagerInterface();
    void requestControlledElementID();

    // For testing.
    bool wirelessVideoPlaybackDisabled();

private:
    friend class PlaybackSessionModelContext;
    friend class VideoFullscreenManagerProxy;

    explicit PlaybackSessionManagerProxy(WebPageProxy&);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    typedef std::tuple<RefPtr<PlaybackSessionModelContext>, RefPtr<PlatformPlaybackSessionInterface>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(PlaybackSessionContextIdentifier);
    ModelInterfaceTuple& ensureModelAndInterface(PlaybackSessionContextIdentifier);
    PlaybackSessionModelContext& ensureModel(PlaybackSessionContextIdentifier);
    PlatformPlaybackSessionInterface& ensureInterface(PlaybackSessionContextIdentifier);
    void addClientForContext(PlaybackSessionContextIdentifier);
    void removeClientForContext(PlaybackSessionContextIdentifier);

    PlaybackSessionContextIdentifier controlsManagerContextId() const { return m_controlsManagerContextId; }

    // Messages from PlaybackSessionManager
    void setUpPlaybackControlsManagerWithID(PlaybackSessionContextIdentifier);
    void clearPlaybackControlsManager();
    void currentTimeChanged(PlaybackSessionContextIdentifier, double currentTime, double hostTime);
    void bufferedTimeChanged(PlaybackSessionContextIdentifier, double bufferedTime);
    void seekableRangesVectorChanged(PlaybackSessionContextIdentifier, Vector<std::pair<double, double>> ranges, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(PlaybackSessionContextIdentifier, bool value);
    void audioMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier, Vector<WebCore::MediaSelectionOption> options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier, Vector<WebCore::MediaSelectionOption> options, uint64_t selectedIndex);
    void audioMediaSelectionIndexChanged(PlaybackSessionContextIdentifier, uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(PlaybackSessionContextIdentifier, uint64_t selectedIndex);
    void externalPlaybackPropertiesChanged(PlaybackSessionContextIdentifier, bool enabled, uint32_t targetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(PlaybackSessionContextIdentifier, bool);
    void durationChanged(PlaybackSessionContextIdentifier, double duration);
    void playbackStartedTimeChanged(PlaybackSessionContextIdentifier, double playbackStartedTime);
    void rateChanged(PlaybackSessionContextIdentifier, bool isPlaying, double rate);
    void handleControlledElementIDResponse(PlaybackSessionContextIdentifier, String) const;
    void mutedChanged(PlaybackSessionContextIdentifier, bool muted);
    void volumeChanged(PlaybackSessionContextIdentifier, double volume);
    void pictureInPictureSupportedChanged(PlaybackSessionContextIdentifier, bool pictureInPictureSupported);

    // Messages to PlaybackSessionManager
    void play(PlaybackSessionContextIdentifier);
    void pause(PlaybackSessionContextIdentifier);
    void togglePlayState(PlaybackSessionContextIdentifier);
    void beginScrubbing(PlaybackSessionContextIdentifier);
    void endScrubbing(PlaybackSessionContextIdentifier);
    void seekToTime(PlaybackSessionContextIdentifier, double time, double before, double after);
    void fastSeek(PlaybackSessionContextIdentifier, double time);
    void beginScanningForward(PlaybackSessionContextIdentifier);
    void beginScanningBackward(PlaybackSessionContextIdentifier);
    void endScanning(PlaybackSessionContextIdentifier);
    void selectAudioMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void selectLegibleMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void togglePictureInPicture(PlaybackSessionContextIdentifier);
    void toggleMuted(PlaybackSessionContextIdentifier);
    void setMuted(PlaybackSessionContextIdentifier, bool);
    void setVolume(PlaybackSessionContextIdentifier, double);
    void setPlayingOnSecondScreen(PlaybackSessionContextIdentifier, bool);

    WebPageProxy* m_page;
    HashMap<PlaybackSessionContextIdentifier, ModelInterfaceTuple> m_contextMap;
    PlaybackSessionContextIdentifier m_controlsManagerContextId;
    HashCountedSet<PlaybackSessionContextIdentifier> m_clientCounts;
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
