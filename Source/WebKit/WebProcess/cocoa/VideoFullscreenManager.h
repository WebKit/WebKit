/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO_PRESENTATION_MODE)

#include "Connection.h"
#include "MessageReceiver.h"
#include "PlaybackSessionContextIdentifier.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/VideoFullscreenModelVideoElement.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/WeakHashMap.h>

namespace IPC {
class Connection;
class Decoder;
class MessageReceiver;
}

namespace WTF {
class MachSendRight;
}

namespace WebCore {
class FloatSize;
class Node;
}

namespace WebKit {

using LayerHostingContextID = uint32_t;

class LayerHostingContext;
class WebPage;
class PlaybackSessionInterfaceContext;
class PlaybackSessionManager;
class ShareableBitmapHandle;
class VideoFullscreenManager;

class VideoFullscreenInterfaceContext
    : public RefCounted<VideoFullscreenInterfaceContext>
    , public WebCore::VideoFullscreenModelClient {
public:
    static Ref<VideoFullscreenInterfaceContext> create(VideoFullscreenManager& manager, PlaybackSessionContextIdentifier contextId)
    {
        return adoptRef(*new VideoFullscreenInterfaceContext(manager, contextId));
    }
    virtual ~VideoFullscreenInterfaceContext();

    void invalidate() { m_manager = nullptr; }

    LayerHostingContext* layerHostingContext() { return m_layerHostingContext.get(); }
    void setLayerHostingContext(std::unique_ptr<LayerHostingContext>&&);

    enum class AnimationType { None, IntoFullscreen, FromFullscreen };
    AnimationType animationState() const { return m_animationType; }
    void setAnimationState(AnimationType flag) { m_animationType = flag; }

    bool targetIsFullscreen() const { return m_targetIsFullscreen; }
    void setTargetIsFullscreen(bool flag) { m_targetIsFullscreen = flag; }

    WebCore::HTMLMediaElementEnums::VideoFullscreenMode fullscreenMode() const { return m_fullscreenMode; }
    void setFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) { m_fullscreenMode = mode; }

    bool fullscreenStandby() const { return m_fullscreenStandby; }
    void setFullscreenStandby(bool value) { m_fullscreenStandby = value; }

    bool isFullscreen() const { return m_isFullscreen; }
    void setIsFullscreen(bool flag) { m_isFullscreen = flag; }

    RetainPtr<CALayer> rootLayer() const { return m_rootLayer; }
    void setRootLayer(RetainPtr<CALayer>);

private:
    // VideoFullscreenModelClient
    void hasVideoChanged(bool) override;
    void videoDimensionsChanged(const WebCore::FloatSize&) override;
    void setPlayerIdentifier(std::optional<WebCore::MediaPlayerIdentifier>) final;

    VideoFullscreenInterfaceContext(VideoFullscreenManager&, PlaybackSessionContextIdentifier);

    VideoFullscreenManager* m_manager;
    PlaybackSessionContextIdentifier m_contextId;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    AnimationType m_animationType { AnimationType::None };
    bool m_targetIsFullscreen { false };
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_fullscreenMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone };
    bool m_fullscreenStandby { false };
    bool m_isFullscreen { false };
    RetainPtr<CALayer> m_rootLayer;
};

class VideoFullscreenManager : public RefCounted<VideoFullscreenManager>, private IPC::MessageReceiver {
public:
    static Ref<VideoFullscreenManager> create(WebPage&, PlaybackSessionManager&);
    virtual ~VideoFullscreenManager();

    void invalidate();

    bool hasVideoPlayingInPictureInPicture() const;

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    void setupRemoteLayerHosting(WebCore::HTMLVideoElement&);
    void willRemoveLayerForID(PlaybackSessionContextIdentifier);

    // Interface to WebChromeClient
    bool canEnterVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool supportsVideoFullscreenStandby() const;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool standby);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WTF::CompletionHandler<void(bool)>&& = [](bool) { });
    void exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);

    void updateTextTrackRepresentationForVideoElement(WebCore::HTMLVideoElement&, ShareableBitmapHandle&&);
    void setTextTrackRepresentationContentScaleForVideoElement(WebCore::HTMLVideoElement&, float scale);
    void setTextTrackRepresentationIsHiddenForVideoElement(WebCore::HTMLVideoElement&, bool hidden);

    bool videoElementInPictureInPicture() const { return !!m_videoElementInPictureInPicture; }

protected:
    friend class VideoFullscreenInterfaceContext;

    explicit VideoFullscreenManager(WebPage&, PlaybackSessionManager&);

    typedef std::tuple<RefPtr<WebCore::VideoFullscreenModelVideoElement>, RefPtr<VideoFullscreenInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(PlaybackSessionContextIdentifier, bool createLayerHostingContext);
    ModelInterfaceTuple& ensureModelAndInterface(PlaybackSessionContextIdentifier, bool createLayerHostingContext = true);
    WebCore::VideoFullscreenModelVideoElement& ensureModel(PlaybackSessionContextIdentifier);
    VideoFullscreenInterfaceContext& ensureInterface(PlaybackSessionContextIdentifier);
    void removeContext(PlaybackSessionContextIdentifier);
    void addClientForContext(PlaybackSessionContextIdentifier);
    void removeClientForContext(PlaybackSessionContextIdentifier);

    // Interface to VideoFullscreenInterfaceContext
    void hasVideoChanged(PlaybackSessionContextIdentifier, bool hasVideo);
    void videoDimensionsChanged(PlaybackSessionContextIdentifier, const WebCore::FloatSize&);
    void setPlayerIdentifier(PlaybackSessionContextIdentifier, std::optional<WebCore::MediaPlayerIdentifier>);

    // Messages from VideoFullscreenManagerProxy
    void requestFullscreenMode(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia);
    void requestUpdateInlineRect(PlaybackSessionContextIdentifier);
    void requestVideoContentLayer(PlaybackSessionContextIdentifier);
    void returnVideoContentLayer(PlaybackSessionContextIdentifier);
#if !PLATFORM(IOS_FAMILY)
    void didSetupFullscreen(PlaybackSessionContextIdentifier);
#endif
    void willExitFullscreen(PlaybackSessionContextIdentifier);
    void didExitFullscreen(PlaybackSessionContextIdentifier);
    void didEnterFullscreen(PlaybackSessionContextIdentifier, std::optional<WebCore::FloatSize>);
    void failedToEnterFullscreen(PlaybackSessionContextIdentifier);
    void didCleanupFullscreen(PlaybackSessionContextIdentifier);
    void setVideoLayerFrameFenced(PlaybackSessionContextIdentifier, WebCore::FloatRect bounds, WTF::MachSendRight&&);
    void setVideoLayerGravityEnum(PlaybackSessionContextIdentifier, unsigned gravity);
    void fullscreenModeChanged(PlaybackSessionContextIdentifier, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(PlaybackSessionContextIdentifier, bool isPageVisible);
    void requestRouteSharingPolicyAndContextUID(PlaybackSessionContextIdentifier, CompletionHandler<void(WebCore::RouteSharingPolicy, String)>&&);

    void setCurrentlyInFullscreen(VideoFullscreenInterfaceContext&, bool);

#if !RELEASE_LOG_DISABLED
    const Logger& logger() const;
    const void* logIdentifier() const;
    const char* logClassName() const;
    WTFLogChannel& logChannel() const;
#endif

    WeakPtr<WebPage> m_page;
    Ref<PlaybackSessionManager> m_playbackSessionManager;
    WeakHashMap<WebCore::HTMLVideoElement, PlaybackSessionContextIdentifier, WebCore::WeakPtrImplWithEventTargetData> m_videoElements;
    HashMap<PlaybackSessionContextIdentifier, ModelInterfaceTuple> m_contextMap;
    PlaybackSessionContextIdentifier m_controlsManagerContextId;
    HashMap<PlaybackSessionContextIdentifier, int> m_clientCounts;
    WeakPtr<WebCore::HTMLVideoElement, WebCore::WeakPtrImplWithEventTargetData> m_videoElementInPictureInPicture;
    bool m_currentlyInFullscreen { false };
    WTF::Function<void(LayerHostingContextID, const WebCore::FloatSize&)> m_setupFullscreenHandler;
};

} // namespace WebKit

#endif // ENABLE(VIDEO_PRESENTATION_MODE)
