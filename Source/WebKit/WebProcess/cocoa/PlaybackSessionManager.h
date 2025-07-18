/*
 * Copyright (C) 2016-2025 Apple Inc. All rights reserved.
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
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/MediaPlayerClientIdentifier.h>
#if HAVE(PIP_SKIP_PREROLL)
#include <WebCore/MediaSession.h>
#endif
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlatformMediaSession.h>
#include <WebCore/PlaybackSessionModelMediaElement.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>

namespace IPC {
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WebCore {
class Node;
enum class AudioSessionSoundStageSize : uint8_t;
}

namespace WebKit {

class WebPage;
class PlaybackSessionManager;

class PlaybackSessionInterfaceContext final
    : public RefCounted<PlaybackSessionInterfaceContext>
    , public WebCore::PlaybackSessionModelClient
    , public CanMakeCheckedPtr<PlaybackSessionInterfaceContext> {
    WTF_MAKE_TZONE_ALLOCATED(PlaybackSessionInterfaceContext);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlaybackSessionInterfaceContext);
public:
    static Ref<PlaybackSessionInterfaceContext> create(PlaybackSessionManager& manager, WebCore::HTMLMediaElementIdentifier contextId)
    {
        return adoptRef(*new PlaybackSessionInterfaceContext(manager, contextId));
    }
    virtual ~PlaybackSessionInterfaceContext();

    void invalidate() { m_manager = nullptr; }

private:
    friend class VideoPresentationInterfaceContext;

    // CheckedPtr interface
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }

    // PlaybackSessionModelClient
    void durationChanged(double) final;
    void currentTimeChanged(double currentTime, double anchorTime) final;
    void bufferedTimeChanged(double) final;
    void playbackStartedTimeChanged(double playbackStartedTime) final;
    void rateChanged(OptionSet<WebCore::PlaybackSessionModel::PlaybackState>, double playbackRate, double defaultPlaybackRate) final;
    void seekableRangesChanged(const WebCore::PlatformTimeRanges&, double lastModifiedTime, double liveUpdateInterval) final;
    void canPlayFastReverseChanged(bool value) final;
    void audioMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex) final;
    void legibleMediaSelectionOptionsChanged(const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex) final;
    void audioMediaSelectionIndexChanged(uint64_t) final;
    void legibleMediaSelectionIndexChanged(uint64_t) final;
    void externalPlaybackChanged(bool enabled, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, const String& localizedDeviceName) final;
    void wirelessVideoPlaybackDisabledChanged(bool) final;
    void mutedChanged(bool) final;
    void volumeChanged(double) final;
    void isPictureInPictureSupportedChanged(bool) final;
    void isInWindowFullscreenActiveChanged(bool) final;
    void spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>&) final;
    void videoProjectionMetadataChanged(const std::optional<WebCore::VideoProjectionMetadata>&) final;

    PlaybackSessionInterfaceContext(PlaybackSessionManager&, WebCore::HTMLMediaElementIdentifier);

    CheckedPtr<PlaybackSessionManager> m_manager;
    WebCore::HTMLMediaElementIdentifier m_contextId;
};

class PlaybackSessionManager
    : public RefCounted<PlaybackSessionManager>
    , private IPC::MessageReceiver
    , public CanMakeCheckedPtr<PlaybackSessionManager>
#if HAVE(PIP_SKIP_PREROLL)
    , public WebCore::MediaSessionObserver
#endif
    {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PlaybackSessionManager);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlaybackSessionManager);
public:
    static Ref<PlaybackSessionManager> create(WebPage&);
    virtual ~PlaybackSessionManager();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&);
    void clearPlaybackControlsManager();
    void mediaEngineChanged(WebCore::HTMLMediaElement&);
    WebCore::HTMLMediaElementIdentifier contextIdForMediaElement(WebCore::HTMLMediaElement&);

    WebCore::HTMLMediaElement* mediaElementWithContextId(WebCore::HTMLMediaElementIdentifier) const;
    WebCore::HTMLMediaElement* currentPlaybackControlsElement() const;

#if HAVE(PIP_SKIP_PREROLL)
    void actionHandlersChanged() final;
#endif

#if !RELEASE_LOG_DISABLED
    void sendLogIdentifierForMediaElement(WebCore::HTMLMediaElement&);
#endif

private:
    friend class PlaybackSessionInterfaceContext;
    friend class VideoPresentationManager;

    explicit PlaybackSessionManager(WebPage&);

    typedef std::tuple<Ref<WebCore::PlaybackSessionModelMediaElement>, Ref<PlaybackSessionInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(WebCore::HTMLMediaElementIdentifier);
    const ModelInterfaceTuple& ensureModelAndInterface(WebCore::HTMLMediaElementIdentifier);
    Ref<WebCore::PlaybackSessionModelMediaElement> ensureModel(WebCore::MediaPlayerClientIdentifier);
    Ref<PlaybackSessionInterfaceContext> ensureInterface(WebCore::MediaPlayerClientIdentifier);
    void removeContext(WebCore::MediaPlayerClientIdentifier);
    void addClientForContext(WebCore::MediaPlayerClientIdentifier);
    void removeClientForContext(WebCore::MediaPlayerClientIdentifier);
#if HAVE(PIP_SKIP_PREROLL)
    void setMediaSessionAndRegisterAsObserver();
#endif

    // Interface to PlaybackSessionInterfaceContext
    void durationChanged(WebCore::MediaPlayerClientIdentifier, double);
    void currentTimeChanged(WebCore::MediaPlayerClientIdentifier, double currentTime, double anchorTime);
    void bufferedTimeChanged(WebCore::MediaPlayerClientIdentifier, double bufferedTime);
    void playbackStartedTimeChanged(WebCore::MediaPlayerClientIdentifier, double playbackStartedTime);
    void rateChanged(WebCore::MediaPlayerClientIdentifier, OptionSet<WebCore::PlaybackSessionModel::PlaybackState>, double playbackRate, double defaultPlaybackRate);
    void seekableRangesChanged(WebCore::MediaPlayerClientIdentifier, const WebCore::PlatformTimeRanges&, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(WebCore::MediaPlayerClientIdentifier, bool value);
    void audioMediaSelectionOptionsChanged(WebCore::MediaPlayerClientIdentifier, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(WebCore::MediaPlayerClientIdentifier, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void audioMediaSelectionIndexChanged(WebCore::MediaPlayerClientIdentifier, uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(WebCore::MediaPlayerClientIdentifier, uint64_t selectedIndex);
    void externalPlaybackChanged(WebCore::MediaPlayerClientIdentifier, bool enabled, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(WebCore::MediaPlayerClientIdentifier, bool);
    void mutedChanged(WebCore::MediaPlayerClientIdentifier, bool);
    void volumeChanged(WebCore::MediaPlayerClientIdentifier, double);
    void isPictureInPictureSupportedChanged(WebCore::MediaPlayerClientIdentifier, bool);
    void isInWindowFullscreenActiveChanged(WebCore::MediaPlayerClientIdentifier, bool);
    void spatialVideoMetadataChanged(WebCore::MediaPlayerClientIdentifier, const std::optional<WebCore::SpatialVideoMetadata>&);
    void videoProjectionMetadataChanged(WebCore::MediaPlayerClientIdentifier, const std::optional<WebCore::VideoProjectionMetadata>&);
#if HAVE(PIP_SKIP_PREROLL)
    void canSkipAdChanged(WebCore::MediaPlayerClientIdentifier, bool);
#endif

    // Messages from PlaybackSessionManagerProxy
    void play(WebCore::MediaPlayerClientIdentifier);
    void pause(WebCore::MediaPlayerClientIdentifier);
    void togglePlayState(WebCore::MediaPlayerClientIdentifier);
    void beginScrubbing(WebCore::MediaPlayerClientIdentifier);
    void endScrubbing(WebCore::MediaPlayerClientIdentifier);
    void seekToTime(WebCore::MediaPlayerClientIdentifier, double time, double toleranceBefore, double toleranceAfter);
    void fastSeek(WebCore::MediaPlayerClientIdentifier, double time);
    void beginScanningForward(WebCore::MediaPlayerClientIdentifier);
    void beginScanningBackward(WebCore::MediaPlayerClientIdentifier);
    void endScanning(WebCore::MediaPlayerClientIdentifier);
    void setDefaultPlaybackRate(WebCore::MediaPlayerClientIdentifier, float);
    void setPlaybackRate(WebCore::MediaPlayerClientIdentifier, float);
    void selectAudioMediaOption(WebCore::MediaPlayerClientIdentifier, uint64_t index);
    void selectLegibleMediaOption(WebCore::MediaPlayerClientIdentifier, uint64_t index);
    void handleControlledElementIDRequest(WebCore::MediaPlayerClientIdentifier);
    void togglePictureInPicture(WebCore::MediaPlayerClientIdentifier);
    void enterFullscreen(WebCore::MediaPlayerClientIdentifier);
    void setPlayerIdentifierForVideoElement(WebCore::MediaPlayerClientIdentifier);
    void exitFullscreen(WebCore::MediaPlayerClientIdentifier);
    void enterInWindow(WebCore::MediaPlayerClientIdentifier);
    void exitInWindow(WebCore::MediaPlayerClientIdentifier);
    void toggleMuted(WebCore::MediaPlayerClientIdentifier);
    void setMuted(WebCore::MediaPlayerClientIdentifier, bool muted);
    void setVolume(WebCore::MediaPlayerClientIdentifier, double volume);
    void setPlayingOnSecondScreen(WebCore::MediaPlayerClientIdentifier, bool value);
    void sendRemoteCommand(WebCore::MediaPlayerClientIdentifier, WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&);
    void setSoundStageSize(WebCore::MediaPlayerClientIdentifier, WebCore::AudioSessionSoundStageSize);
#if HAVE(PIP_SKIP_PREROLL)
    void skipAd(WebCore::MediaPlayerClientIdentifier);
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    void setSpatialTrackingLabel(WebCore::MediaPlayerClientIdentifier, const String&);
#endif

    void forEachModel(Function<void(WebCore::PlaybackSessionModel&)>&&);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const { return m_logger; }
    uint64_t logIdentifier() const { return m_logIdentifier; }
    ASCIILiteral logClassName() const { return "VideoPresentationManager"_s; }
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<WebPage> m_page;
    WeakHashSet<WebCore::HTMLMediaElement> m_mediaElements;
    HashMap<WebCore::MediaPlayerClientIdentifier, ModelInterfaceTuple> m_contextMap;
    Markable<WebCore::MediaPlayerClientIdentifier> m_controlsManagerContextId;
    HashCountedSet<WebCore::MediaPlayerClientIdentifier> m_clientCounts;
#if HAVE(PIP_SKIP_PREROLL)
    WeakPtr<WebCore::MediaSession> m_mediaSession;
    bool m_canSkipAd { false };
#endif

#if !RELEASE_LOG_DISABLED
    const Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
