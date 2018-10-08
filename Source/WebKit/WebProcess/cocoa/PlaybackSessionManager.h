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
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/PlaybackSessionInterface.h>
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
    , public WebCore::PlaybackSessionInterface
    , public WebCore::PlaybackSessionModelClient {
public:
    static Ref<PlaybackSessionInterfaceContext> create(PlaybackSessionManager& manager, uint64_t contextId)
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

    PlaybackSessionInterfaceContext(PlaybackSessionManager&, uint64_t contextId);

    PlaybackSessionManager* m_manager;
    uint64_t m_contextId;
};

class PlaybackSessionManager : public RefCounted<PlaybackSessionManager>, private IPC::MessageReceiver {
public:
    static Ref<PlaybackSessionManager> create(WebPage&);
    virtual ~PlaybackSessionManager();
    
    void invalidate();

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;

    void setUpPlaybackControlsManager(WebCore::HTMLMediaElement&);
    void clearPlaybackControlsManager();
    uint64_t contextIdForMediaElement(WebCore::HTMLMediaElement&);

    WebCore::HTMLMediaElement* currentPlaybackControlsElement() const;

protected:
    friend class PlaybackSessionInterfaceContext;
    friend class VideoFullscreenManager;

    explicit PlaybackSessionManager(WebPage&);

    typedef std::tuple<RefPtr<WebCore::PlaybackSessionModelMediaElement>, RefPtr<PlaybackSessionInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebCore::PlaybackSessionModelMediaElement& ensureModel(uint64_t contextId);
    PlaybackSessionInterfaceContext& ensureInterface(uint64_t contextId);
    void removeContext(uint64_t contextId);
    void addClientForContext(uint64_t contextId);
    void removeClientForContext(uint64_t contextId);

    // Interface to PlaybackSessionInterfaceContext
    void durationChanged(uint64_t contextId, double);
    void currentTimeChanged(uint64_t contextId, double currentTime, double anchorTime);
    void bufferedTimeChanged(uint64_t contextId, double bufferedTime);
    void playbackStartedTimeChanged(uint64_t contextId, double playbackStartedTime);
    void rateChanged(uint64_t contextId, bool isPlaying, float playbackRate);
    void seekableRangesChanged(uint64_t contextId, const WebCore::TimeRanges&, double lastModifiedTime, double liveUpdateInterval);
    void canPlayFastReverseChanged(uint64_t contextId, bool value);
    void audioMediaSelectionOptionsChanged(uint64_t contextId, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void legibleMediaSelectionOptionsChanged(uint64_t contextId, const Vector<WebCore::MediaSelectionOption>& options, uint64_t selectedIndex);
    void audioMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex);
    void legibleMediaSelectionIndexChanged(uint64_t contextId, uint64_t selectedIndex);
    void externalPlaybackChanged(uint64_t contextId, bool enabled, WebCore::PlaybackSessionModel::ExternalPlaybackTargetType, String localizedDeviceName);
    void wirelessVideoPlaybackDisabledChanged(uint64_t contextId, bool);
    void mutedChanged(uint64_t contextId, bool);
    void volumeChanged(uint64_t contextId, double);
    void isPictureInPictureSupportedChanged(uint64_t contextId, bool);

    // Messages from PlaybackSessionManagerProxy
    void play(uint64_t contextId);
    void pause(uint64_t contextId);
    void togglePlayState(uint64_t contextId);
    void beginScrubbing(uint64_t contextId);
    void endScrubbing(uint64_t contextId);
    void seekToTime(uint64_t contextId, double time, double toleranceBefore, double toleranceAfter);
    void fastSeek(uint64_t contextId, double time);
    void beginScanningForward(uint64_t contextId);
    void beginScanningBackward(uint64_t contextId);
    void endScanning(uint64_t contextId);
    void selectAudioMediaOption(uint64_t contextId, uint64_t index);
    void selectLegibleMediaOption(uint64_t contextId, uint64_t index);
    void handleControlledElementIDRequest(uint64_t contextId);
    void togglePictureInPicture(uint64_t contextId);
    void toggleMuted(uint64_t contextId);
    void setMuted(uint64_t contextId, bool muted);
    void setVolume(uint64_t contextId, double volume);
    void setPlayingOnSecondScreen(uint64_t contextId, bool value);

    WebPage* m_page;
    HashMap<WebCore::HTMLMediaElement*, uint64_t> m_mediaElements;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
    uint64_t m_controlsManagerContextId { 0 };
    HashCountedSet<uint64_t> m_clientCounts;
};

} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
