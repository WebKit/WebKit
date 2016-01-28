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
#ifndef WebVideoFullscreenManager_h
#define WebVideoFullscreenManager_h

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElementEnums.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/WebVideoFullscreenInterface.h>
#include <WebCore/WebVideoFullscreenModelVideoElement.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Attachment;
class Connection;
class MessageDecoder;
class MessageReceiver;
}

namespace WebCore {
class Node;
}

namespace WebKit {

class LayerHostingContext;
class WebPage;
class WebVideoFullscreenManager;

class WebVideoFullscreenInterfaceContext : public RefCounted<WebVideoFullscreenInterfaceContext>, public WebCore::WebVideoFullscreenInterface {
public:
    static Ref<WebVideoFullscreenInterfaceContext> create(WebVideoFullscreenManager& manager, uint64_t contextId)
    {
        return adoptRef(*new WebVideoFullscreenInterfaceContext(manager, contextId));
    }
    virtual ~WebVideoFullscreenInterfaceContext();

    void invalidate() { m_manager = nullptr; }

    LayerHostingContext* layerHostingContext() { return m_layerHostingContext.get(); }
    void setLayerHostingContext(std::unique_ptr<LayerHostingContext>&&);

    bool isAnimating() const { return m_isAnimating; }
    void setIsAnimating(bool flag) { m_isAnimating = flag; }

    bool targetIsFullscreen() const { return m_targetIsFullscreen; }
    void setTargetIsFullscreen(bool flag) { m_targetIsFullscreen = flag; }

    WebCore::HTMLMediaElementEnums::VideoFullscreenMode fullscreenMode() const { return m_fullscreenMode; }
    void setFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode mode) { m_fullscreenMode = mode; }

    bool isFullscreen() const { return m_isFullscreen; }
    void setIsFullscreen(bool flag) { m_isFullscreen = flag; }

private:
    // WebVideoFullscreenInterface
    virtual void resetMediaState() override;
    virtual void setDuration(double) override;
    virtual void setCurrentTime(double currentTime, double anchorTime) override;
    virtual void setBufferedTime(double) override;
    virtual void setRate(bool isPlaying, float playbackRate) override;
    virtual void setVideoDimensions(bool hasVideo, float width, float height) override;
    virtual void setSeekableRanges(const WebCore::TimeRanges&) override;
    virtual void setCanPlayFastReverse(bool value) override;
    virtual void setAudioMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex) override;
    virtual void setLegibleMediaSelectionOptions(const Vector<WTF::String>& options, uint64_t selectedIndex) override;
    virtual void setExternalPlayback(bool enabled, ExternalPlaybackTargetType, WTF::String localizedDeviceName) override;
    virtual void setWirelessVideoPlaybackDisabled(bool) override;

    WebVideoFullscreenInterfaceContext(WebVideoFullscreenManager&, uint64_t contextId);

    WebVideoFullscreenManager* m_manager;
    uint64_t m_contextId;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    bool m_isAnimating { false };
    bool m_targetIsFullscreen { false };
    WebCore::HTMLMediaElementEnums::VideoFullscreenMode m_fullscreenMode { WebCore::HTMLMediaElementEnums::VideoFullscreenModeNone };
    bool m_isFullscreen { false };
};

class WebVideoFullscreenManager : public RefCounted<WebVideoFullscreenManager>, private IPC::MessageReceiver {
public:
    static Ref<WebVideoFullscreenManager> create(PassRefPtr<WebPage>);
    virtual ~WebVideoFullscreenManager();
    
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    // Interface to ChromeClient
    bool supportsVideoFullscreen() const;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&);
    
protected:
    friend class WebVideoFullscreenInterfaceContext;

    explicit WebVideoFullscreenManager(PassRefPtr<WebPage>);

    typedef std::tuple<RefPtr<WebCore::WebVideoFullscreenModelVideoElement>, RefPtr<WebVideoFullscreenInterfaceContext>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebCore::WebVideoFullscreenModelVideoElement& ensureModel(uint64_t contextId);
    WebVideoFullscreenInterfaceContext& ensureInterface(uint64_t contextId);

    // Interface to WebVideoFullscreenInterfaceContext
    void resetMediaState(uint64_t contextId);
    void setDuration(uint64_t contextId, double);
    void setCurrentTime(uint64_t contextId, double currentTime, double anchorTime);
    void setBufferedTime(uint64_t contextId, double bufferedTime);
    void setRate(uint64_t contextId, bool isPlaying, float playbackRate);
    void setVideoDimensions(uint64_t contextId, bool hasVideo, float width, float height);
    void setSeekableRanges(uint64_t contextId, const WebCore::TimeRanges&);
    void setCanPlayFastReverse(uint64_t contextId, bool value);
    void setAudioMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex);
    void setLegibleMediaSelectionOptions(uint64_t contextId, const Vector<String>& options, uint64_t selectedIndex);
    void setExternalPlayback(uint64_t contextId, bool enabled, WebCore::WebVideoFullscreenInterface::ExternalPlaybackTargetType, String localizedDeviceName);
    void setWirelessVideoPlaybackDisabled(uint64_t contextId, bool);

    // Messages from WebVideoFullscreenManagerProxy
    void play(uint64_t contextId);
    void pause(uint64_t contextId);
    void togglePlayState(uint64_t contextId);
    void beginScrubbing(uint64_t contextId);
    void endScrubbing(uint64_t contextId);
    void seekToTime(uint64_t contextId, double time);
    void fastSeek(uint64_t contextId, double time);
    void beginScanningForward(uint64_t contextId);
    void beginScanningBackward(uint64_t contextId);
    void endScanning(uint64_t contextId);
    void requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void didSetupFullscreen(uint64_t contextId);
    void didExitFullscreen(uint64_t contextId);
    void didEnterFullscreen(uint64_t contextId);
    void didCleanupFullscreen(uint64_t contextId);
    void setVideoLayerFrameFenced(uint64_t contextId, WebCore::FloatRect bounds, IPC::Attachment fencePort);
    void setVideoLayerGravityEnum(uint64_t contextId, unsigned gravity);
    void selectAudioMediaOption(uint64_t contextId, uint64_t index);
    void selectLegibleMediaOption(uint64_t contextId, uint64_t index);
    void fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(uint64_t contextId, bool isPageVisible);
    
    WebPage* m_page;
    HashMap<WebCore::HTMLVideoElement*, uint64_t> m_videoElements;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#endif // WebVideoFullscreenManager_h
