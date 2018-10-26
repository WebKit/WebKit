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

#if (PLATFORM(IOS_FAMILY) && HAVE(AVKIT)) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/VideoFullscreenModelVideoElement.h>
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
class FloatSize;
class Node;
}

namespace WebKit {

class LayerHostingContext;
class WebPage;
class PlaybackSessionInterfaceContext;
class PlaybackSessionManager;
class VideoFullscreenManager;

class VideoFullscreenInterfaceContext
    : public RefCounted<VideoFullscreenInterfaceContext>
    , public WebCore::VideoFullscreenModelClient {
public:
    static Ref<VideoFullscreenInterfaceContext> create(VideoFullscreenManager& manager, uint64_t contextId)
    {
        return adoptRef(*new VideoFullscreenInterfaceContext(manager, contextId));
    }
    virtual ~VideoFullscreenInterfaceContext();

    void invalidate() { m_manager = nullptr; }

    LayerHostingContext* layerHostingContext() { return m_layerHostingContext.get(); }
    void setLayerHostingContext(std::unique_ptr<LayerHostingContext>&&);

    bool isAnimating() const { return m_isAnimating; }
    void setIsAnimating(bool flag) { m_isAnimating = flag; }

    bool targetIsFullscreen() const { return m_targetIsFullscreen; }
    void setTargetIsFullscreen(bool flag) { m_targetIsFullscreen = flag; }

    WebCore::HTMLMediaElementEnums::VideoFullscreenMode fullscreenMode() const { return m_fullscreenMode; }
    void setFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) { m_fullscreenMode = mode; }

    bool fullscreenStandby() const { return m_fullscreenStandby; }
    void setFullscreenStandby(bool value) { m_fullscreenStandby = value; }

    bool isFullscreen() const { return m_isFullscreen; }
    void setIsFullscreen(bool flag) { m_isFullscreen = flag; }

private:
    // VideoFullscreenModelClient
    void hasVideoChanged(bool) override;
    void videoDimensionsChanged(const WebCore::FloatSize&) override;

    VideoFullscreenInterfaceContext(VideoFullscreenManager&, uint64_t contextId);

    VideoFullscreenManager* m_manager;
    uint64_t m_contextId;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    bool m_isAnimating { false };
    bool m_targetIsFullscreen { false };
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_fullscreenMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone };
    bool m_fullscreenStandby { false };
    bool m_isFullscreen { false };
};

class VideoFullscreenManager : public RefCounted<VideoFullscreenManager>, private IPC::MessageReceiver {
public:
    static Ref<VideoFullscreenManager> create(WebPage&, PlaybackSessionManager&);
    virtual ~VideoFullscreenManager();
    
    void invalidate();
    
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;

    // Interface to ChromeClient
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool supportsVideoFullscreenStandby() const;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool standby);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&);
    void exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);

protected:
    friend class VideoFullscreenInterfaceContext;

    explicit VideoFullscreenManager(WebPage&, PlaybackSessionManager&);

    typedef std::tuple<RefPtr<WebCore::VideoFullscreenModelVideoElement>, RefPtr<VideoFullscreenInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebCore::VideoFullscreenModelVideoElement& ensureModel(uint64_t contextId);
    VideoFullscreenInterfaceContext& ensureInterface(uint64_t contextId);
    void removeContext(uint64_t contextId);
    void addClientForContext(uint64_t contextId);
    void removeClientForContext(uint64_t contextId);

    // Interface to VideoFullscreenInterfaceContext
    void hasVideoChanged(uint64_t contextId, bool hasVideo);
    void videoDimensionsChanged(uint64_t contextId, const WebCore::FloatSize&);

    // Messages from VideoFullscreenManagerProxy
    void requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool finishedWithMedia);
    void requestUpdateInlineRect(uint64_t contextId);
    void requestVideoContentLayer(uint64_t contextId);
    void returnVideoContentLayer(uint64_t contextId);
    void didSetupFullscreen(uint64_t contextId);
    void willExitFullscreen(uint64_t contextId);
    void didExitFullscreen(uint64_t contextId);
    void didEnterFullscreen(uint64_t contextId);
    void didCleanupFullscreen(uint64_t contextId);
    void setVideoLayerFrameFenced(uint64_t contextId, WebCore::FloatRect bounds, IPC::Attachment fencePort);
    void setVideoLayerGravityEnum(uint64_t contextId, unsigned gravity);
    void fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(uint64_t contextId, bool isPageVisible);
    
    WebPage* m_page;
    Ref<PlaybackSessionManager> m_playbackSessionManager;
    HashMap<WebCore::HTMLVideoElement*, uint64_t> m_videoElements;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
    uint64_t m_controlsManagerContextId { 0 };
    HashMap<uint64_t, int> m_clientCounts;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

