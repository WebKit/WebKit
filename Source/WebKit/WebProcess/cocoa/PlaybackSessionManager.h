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
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlaybackSessionModelMediaElement.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Attachment;
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WebCore {
class Node;
}

namespace WebKit {

class WebPage;
class PlaybackSessionManager;

class PlaybackSessionInterfaceContext final
    : public RefCounted<PlaybackSessionInterfaceContext>
    , public WebCore::PlaybackSessionModelClient {
public:
    static Ref<PlaybackSessionInterfaceContext> create(PlaybackSessionManager& manager, PlaybackSessionContextIdentifier contextId)
    {
        return adoptRef(*new PlaybackSessionInterfaceContext(manager, contextId));
    }
    virtual ~PlaybackSessionInterfaceContext();

    void invalidate() { m_manager = nullptr; }

private:
    friend class VideoFullscreenInterfaceContext;

    // PlaybackSessionModelClient
    void durationChanged(double) final;
    void currentTimeChanged(double currentTime, double anchorTime) final;
    void bufferedTimeChanged(double) final;
    void playbackStartedTimeChanged(double playbackStartedTime) final;
    void rateChanged(bool isPlaying, float playbackRate) final;
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

    PlaybackSessionInterfaceContext(PlaybackSessionManager&, PlaybackSessionContextIdentifier);

    PlaybackSessionManager* m_manager;
    PlaybackSessionContextIdentifier m_contextId;
};

class PlaybackSessionManager : public RefCounted<PlaybackSessionManager>, private IPC::MessageReceiver {
public:
    static Ref<PlaybackSessionManager> create(WebPage&);
    virtual ~PlaybackSessionManager();
    
    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&);
    void clearPlaybackControlsManager();
    PlaybackSessionContextIdentifier contextIdForMediaElement(WebCore::HTMLMediaElement&);

    WebCore::HTMLMediaElement* currentPlaybackControlsElement() const;

protected:
    friend class PlaybackSessionInterfaceContext;
    friend class VideoFullscreenManager;

    explicit PlaybackSessionManager(WebPage&);

    typedef std::tuple<RefPtr<WebCore::PlaybackSessionModelMediaElement>, RefPtr<PlaybackSessionInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(PlaybackSessionContextIdentifier);
    ModelInterfaceTuple& ensureModelAndInterface(PlaybackSessionContextIdentifier);
    WebCore::PlaybackSessionModelMediaElement& ensureModel(PlaybackSessionContextIdentifier);
    PlaybackSessionInterfaceContext& ensureInterface(PlaybackSessionContextIdentifier);
    void removeContext(PlaybackSessionContextIdentifier);
    void addClientForContext(PlaybackSessionContextIdentifier);
    void removeClientForContext(PlaybackSessionContextIdentifier);

    // Interface to PlaybackSessionInterfaceContext
    void durationChanged(PlaybackSessionContextIdentifier, double);
    void currentTimeChanged(PlaybackSessionContextIdentifier, double currentTime, double anchorTime);
    void bufferedTimeChanged(PlaybackSessionContextIdentifier, double bufferedTime);
    void playbackStartedTimeChanged(PlaybackSessionContextIdentifier, double playbackStartedTime);
    void rateChanged(PlaybackSessionContextIdentifier, bool isPlaying, float playbackRate);
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
    void selectAudioMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void selectLegibleMediaOption(PlaybackSessionContextIdentifier, uint64_t index);
    void handleControlledElementIDRequest(PlaybackSessionContextIdentifier);
    void togglePictureInPicture(PlaybackSessionContextIdentifier);
    void toggleMuted(PlaybackSessionContextIdentifier);
    void setMuted(PlaybackSessionContextIdentifier, bool muted);
    void setVolume(PlaybackSessionContextIdentifier, double volume);
    void setPlayingOnSecondScreen(PlaybackSessionContextIdentifier, bool value);

    WebPage* m_page;
    HashMap<WebCore::HTMLMediaElement*, PlaybackSessionContextIdentifier> m_mediaElements;
    HashMap<PlaybackSessionContextIdentifier, ModelInterfaceTuple> m_contextMap;
    PlaybackSessionContextIdentifier m_controlsManagerContextId;
    HashCountedSet<PlaybackSessionContextIdentifier> m_clientCounts;
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
