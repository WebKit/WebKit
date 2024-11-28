/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
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
    static Ref<PlaybackSessionInterfaceContext> create(PlaybackSessionManager& manager, PlaybackSessionContextIdentifier contextId)
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
    void seekableRangesChanged(const WebCore::TimeRanges&, double lastModifiedTime, double liveUpdateInterval) final;
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
#if ENABLE(LINEAR_MEDIA_PLAYER)
    void spatialVideoMetadataChanged(const std::optional<WebCore::SpatialVideoMetadata>&) final;
#endif

    PlaybackSessionInterfaceContext(PlaybackSessionManager&, PlaybackSessionContextIdentifier);

    CheckedPtr<PlaybackSessionManager> m_manager;
    PlaybackSessionContextIdentifier m_contextId;
};

class PlaybackSessionManager : public RefCounted<PlaybackSessionManager>, private IPC::MessageReceiver, public CanMakeCheckedPtr<PlaybackSessionManager> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PlaybackSessionManager);
public:
    static Ref<PlaybackSessionManager> create(WebPage&);
    virtual ~PlaybackSessionManager();

    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&);
    void clearPlaybackControlsManager();
    void mediaEngineChanged(WebCore::HTMLMediaElement&);
    PlaybackSessionContextIdentifier contextIdForMediaElement(WebCore::HTMLMediaElement&);

    WebCore::HTMLMediaElement* currentPlaybackControlsElement() const;

#if !RELEASE_LOG_DISABLED
    void sendLogIdentifierForMediaElement(WebCore::HTMLMediaElement&);
#endif

private:
    friend class PlaybackSessionInterfaceContext;
    friend class VideoPresentationManager;

    explicit PlaybackSessionManager(WebPage&);

    typedef std::tuple<Ref<WebCore::PlaybackSessionModelMediaElement>, Ref<PlaybackSessionInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(PlaybackSessionContextIdentifier);
    const ModelInterfaceTuple& ensureModelAndInterface(PlaybackSessionContextIdentifier);
    Ref<WebCore::PlaybackSessionModelMediaElement> ensureModel(PlaybackSessionContextIdentifier);
    Ref<PlaybackSessionInterfaceContext> ensureInterface(PlaybackSessionContextIdentifier);
    void removeContext(PlaybackSessionContextIdentifier);
    void addClientForContext(PlaybackSessionContextIdentifier);
    void removeClientForContext(PlaybackSessionContextIdentifier);

    // Interface to PlaybackSessionInterfaceContext
    void durationChanged(PlaybackSessionContextIdentifier, double);
    void currentTimeChanged(PlaybackSessionContextIdentifier, double currentTime, double anchorTime);
    void bufferedTimeChanged(PlaybackSessionContextIdentifier, double bufferedTime);
    void playbackStartedTimeChanged(PlaybackSessionContextIdentifier, double playbackStartedTime);
    void rateChanged(PlaybackSessionContextIdentifier, OptionSet<WebCore::PlaybackSessionModel::PlaybackState>, double playbackRate, double defaultPlaybackRate);
    void seekableRangesChanged(PlaybackSessionContextIdentifier, const WebCore::TimeRanges&, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(PlaybackSessionContextIdentifier, bool value);
    void audioMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(PlaybackSessionContextIdentifier, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void audioMediaSelectionIndexChanged(PlaybackSessionContextIdentifier, uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(PlaybackSessionContextIdentifier, uint64_t selectedIndex);
    void externalPlaybackChanged(PlaybackSessionContextIdentifier, bool enabled, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(PlaybackSessionContextIdentifier, bool);
    void mutedChanged(PlaybackSessionContextIdentifier, bool);
    void volumeChanged(PlaybackSessionContextIdentifier, double);
    void isPictureInPictureSupportedChanged(PlaybackSessionContextIdentifier, bool);
    void isInWindowFullscreenActiveChanged(PlaybackSessionContextIdentifier, bool);
    void spatialVideoMetadataChanged(PlaybackSessionContextIdentifier, const std::optional<WebCore::SpatialVideoMetadata>&);

    // Messages from PlaybackSessionManagerProxy
    void play(PlaybackSessionContextIdentifier);
    void pause(PlaybackSessionContextIdentifier);
    void togglePlayState(PlaybackSessionContextIdentifier);
    void beginScrubbing(PlaybackSessionContextIdentifier);
    void endScrubbing(PlaybackSessionContextIdentifier);
    void seekToTime(PlaybackSessionContextIdentifier, double time, double toleranceBefore, double toleranceAfter);
    void fastSeek(PlaybackSessionContextIdentifier, double time);
    void beginScanningForward(PlaybackSessionContextIdentifier);
    void beginScanningBackward(PlaybackSessionContextIdentifier);
    void endScanning(PlaybackSessionContextIdentifier);
    void setDefaultPlaybackRate(PlaybackSessionContextIdentifier, float);
    void setPlaybackRate(PlaybackSessionContextIdentifier, float);
    void selectAudioMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void selectLegibleMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void handleControlledElementIDRequest(PlaybackSessionContextIdentifier);
    void togglePictureInPicture(PlaybackSessionContextIdentifier);
    void enterFullscreen(PlaybackSessionContextIdentifier);
    void exitFullscreen(PlaybackSessionContextIdentifier);
    void enterInWindow(PlaybackSessionContextIdentifier);
    void exitInWindow(PlaybackSessionContextIdentifier);
    void toggleMuted(PlaybackSessionContextIdentifier);
    void setMuted(PlaybackSessionContextIdentifier, bool muted);
    void setVolume(PlaybackSessionContextIdentifier, double volume);
    void setPlayingOnSecondScreen(PlaybackSessionContextIdentifier, bool value);
    void sendRemoteCommand(PlaybackSessionContextIdentifier, WebCore::PlatformMediaSession::RemoteControlCommandType, const WebCore::PlatformMediaSession::RemoteCommandArgument&);
    void setSoundStageSize(PlaybackSessionContextIdentifier, WebCore::AudioSessionSoundStageSize);

#if HAVE(SPATIAL_TRACKING_LABEL)
    void setSpatialTrackingLabel(PlaybackSessionContextIdentifier, const String&);
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
    HashMap<PlaybackSessionContextIdentifier, ModelInterfaceTuple> m_contextMap;
    Markable<PlaybackSessionContextIdentifier> m_controlsManagerContextId;
    HashCountedSet<PlaybackSessionContextIdentifier> m_clientCounts;

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const uint64_t m_logIdentifier;
#endif
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
