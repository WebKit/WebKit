/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/MediaSelectionOption.h>
#include <WebCore/PlatformView.h>
#include <WebCore/PlaybackSessionModel.h>
#include <WebCore/TimeRanges.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtrContainer.h>

#if PLATFORM(IOS)
#include <WebCore/PlaybackSessionInterfaceAVKit.h>
#else
#include <WebCore/PlaybackSessionInterfaceMac.h>
#endif

#if PLATFORM(IOS)
typedef WebCore::PlaybackSessionInterfaceAVKit PlatformPlaybackSessionInterface;
#else
typedef WebCore::PlaybackSessionInterfaceMac PlatformPlaybackSessionInterface;
#endif

namespace WebKit {

class WebPageProxy;
class PlaybackSessionManagerProxy;

class PlaybackSessionModelContext final
    : public WebCore::PlaybackSessionModel
    , public RefCounted<PlaybackSessionModelContext> {
public:
    static Ref<PlaybackSessionModelContext> create(WeakPtr<PlaybackSessionManagerProxy>&& manager, uint64_t contextId)
    {
        return adoptRef(*new PlaybackSessionModelContext(WTFMove(manager), contextId));
    }
    virtual ~PlaybackSessionModelContext() { }

    void invalidate() { m_manager = nullptr; }

    // PlaybackSessionModel
    void addClient(WeakPtr<WebCore::PlaybackSessionModelClient>&&) final;
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

    using RefCounted::ref;
    using RefCounted::deref;

private:
    friend class VideoFullscreenModelContext;

    PlaybackSessionModelContext(WeakPtr<PlaybackSessionManagerProxy>&& manager, uint64_t contextId)
        : m_manager(WTFMove(manager))
        , m_contextId(contextId)
    {
    }

    // PlaybackSessionModel
    void refPlaybackSessionModel() final { ref(); }
    void derefPlaybackSessionModel() final { deref(); }
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
    bool wirelessVideoPlaybackDisabled() const final { return m_wirelessVideoPlaybackDisabled; }
    bool isMuted() const final { return m_muted; }
    double volume() const final { return m_volume; }
    bool isPictureInPictureSupported() const final { return m_pictureInPictureSupported; }
    bool isPictureInPictureActive() const final { return m_pictureInPictureActive; }
    
    WeakPtr<PlaybackSessionManagerProxy> m_manager;
    uint64_t m_contextId;
    WeakPtrContainer<WebCore::PlaybackSessionModelClient> m_clients;
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

class PlaybackSessionManagerProxy
    : public RefCounted<PlaybackSessionManagerProxy>
    , private IPC::MessageReceiver
    , public CanMakeWeakPtr<PlaybackSessionManagerProxy> {
public:
    static RefPtr<PlaybackSessionManagerProxy> create(WebPageProxy&);
    virtual ~PlaybackSessionManagerProxy();

    void invalidate();

    PlatformPlaybackSessionInterface* controlsManagerInterface();
    void requestControlledElementID();

private:
    friend class PlaybackSessionModelContext;
    friend class VideoFullscreenManagerProxy;

    explicit PlaybackSessionManagerProxy(WebPageProxy&);
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    typedef std::tuple<RefPtr<PlaybackSessionModelContext>, RefPtr<PlatformPlaybackSessionInterface>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    PlaybackSessionModelContext& ensureModel(uint64_t contextId);
    PlatformPlaybackSessionInterface& ensureInterface(uint64_t contextId);
    void addClientForContext(uint64_t contextId);
    void removeClientForContext(uint64_t contextId);

    uint64_t controlsManagerContextId() const { return m_controlsManagerContextId; }

    // Messages from PlaybackSessionManager
    void setUpPlaybackControlsManagerWithID(uint64_t contextId);
    void clearPlaybackControlsManager();
    void resetMediaState(uint64_t contextId);
    void currentTimeChanged(uint64_t contextId, double currentTime, double hostTime);
    void bufferedTimeChanged(uint64_t contextId, double bufferedTime);
    void seekableRangesVectorChanged(uint64_t contextId, Vector<std::pair<double, double>> ranges, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(uint64_t contextId, bool value);
    void audioMediaSelectionOptionsChanged(uint64_t contextId, Vector<WebCore::MediaSelectionOption> options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(uint64_t contextId, Vector<WebCore::MediaSelectionOption> options, uint64_t selectedIndex);
    void audioMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex);
    void externalPlaybackPropertiesChanged(uint64_t contextId, bool enabled, uint32_t targetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(uint64_t contextId, bool);
    void durationChanged(uint64_t contextId, double duration);
    void playbackStartedTimeChanged(uint64_t contextId, double playbackStartedTime);
    void rateChanged(uint64_t contextId, bool isPlaying, double rate);
    void handleControlledElementIDResponse(uint64_t, String) const;
    void mutedChanged(uint64_t contextId, bool muted);
    void volumeChanged(uint64_t contextId, double volume);
    void pictureInPictureSupportedChanged(uint64_t contextId, bool pictureInPictureSupported);
    void pictureInPictureActiveChanged(uint64_t contextId, bool pictureInPictureActive);

    // Messages to PlaybackSessionManager
    void play(uint64_t contextId);
    void pause(uint64_t contextId);
    void togglePlayState(uint64_t contextId);
    void beginScrubbing(uint64_t contextId);
    void endScrubbing(uint64_t contextId);
    void seekToTime(uint64_t contextId, double time, double before, double after);
    void fastSeek(uint64_t contextId, double time);
    void beginScanningForward(uint64_t contextId);
    void beginScanningBackward(uint64_t contextId);
    void endScanning(uint64_t contextId);
    void selectAudioMediaOption(uint64_t contextId, uint64_t index);
    void selectLegibleMediaOption(uint64_t contextId, uint64_t index);
    void togglePictureInPicture(uint64_t contextId);
    void toggleMuted(uint64_t contextId);
    void setMuted(uint64_t contextId, bool);
    void setVolume(uint64_t contextId, double);
    void setPlayingOnSecondScreen(uint64_t contextId, bool);

    WebPageProxy* m_page;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
    uint64_t m_controlsManagerContextId { 0 };
    HashCountedSet<uint64_t> m_clientCounts;
};

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
