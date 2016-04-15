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

#ifndef WebVideoFullscreenManagerProxy_h
#define WebVideoFullscreenManagerProxy_h

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#include "MessageReceiver.h"
#include <WebCore/GraphicsLayer.h>
#include <WebCore/PlatformView.h>
#include <WebCore/WebVideoFullscreenChangeObserver.h>
#include <WebCore/WebVideoFullscreenModel.h>
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if PLATFORM(IOS)
#include <WebCore/WebVideoFullscreenInterfaceAVKit.h>
#else
#include <WebCore/WebVideoFullscreenInterfaceMac.h>
#endif

#if PLATFORM(IOS)
typedef WebCore::WebVideoFullscreenInterfaceAVKit PlatformWebVideoFullscreenInterface;
#else
typedef WebCore::WebVideoFullscreenInterfaceMac PlatformWebVideoFullscreenInterface;
#endif

namespace WebKit {

class WebPageProxy;
class WebPlaybackSessionManagerProxy;
class WebPlaybackSessionModelContext;
class WebVideoFullscreenManagerProxy;

class WebVideoFullscreenModelContext final: public RefCounted<WebVideoFullscreenModelContext>, public WebCore::WebVideoFullscreenModel, public WebCore::WebVideoFullscreenChangeObserver  {
public:
    static Ref<WebVideoFullscreenModelContext> create(WebVideoFullscreenManagerProxy& manager, WebPlaybackSessionModelContext& playbackSessionModel, uint64_t contextId)
    {
        return adoptRef(*new WebVideoFullscreenModelContext(manager, playbackSessionModel, contextId));
    }
    virtual ~WebVideoFullscreenModelContext();

    void invalidate() { m_manager = nullptr; }

    PlatformView *layerHostView() const { return m_layerHostView.get(); }
    void setLayerHostView(RetainPtr<PlatformView>&& layerHostView) { m_layerHostView = WTFMove(layerHostView); }

private:
    WebVideoFullscreenModelContext(WebVideoFullscreenManagerProxy&, WebPlaybackSessionModelContext&, uint64_t);

    // WebVideoFullscreenModel
    void play() override;
    void pause() override;
    void togglePlayState() override;
    void beginScrubbing() override;
    void endScrubbing() override;
    void seekToTime(double) override;
    void fastSeek(double time) override;
    void beginScanningForward() override;
    void beginScanningBackward() override;
    void endScanning() override;
    void requestFullscreenMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    void setVideoLayerFrame(WebCore::FloatRect) override;
    void setVideoLayerGravity(VideoGravity) override;
    void selectAudioMediaOption(uint64_t) override;
    void selectLegibleMediaOption(uint64_t) override;
    void fullscreenModeChanged(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    bool isVisible() const override;

    // WebVideoFullscreenChangeObserver
    void didSetupFullscreen() override;
    void didEnterFullscreen() override;
    void didExitFullscreen() override;
    void didCleanupFullscreen() override;
    void fullscreenMayReturnToInline() override;

    WebVideoFullscreenManagerProxy* m_manager;
    Ref<WebPlaybackSessionModelContext> m_playbackSessionModel;
    uint64_t m_contextId;
    RetainPtr<PlatformView *> m_layerHostView;
};

class WebVideoFullscreenManagerProxy : public RefCounted<WebVideoFullscreenManagerProxy>, private IPC::MessageReceiver {
public:
    static RefPtr<WebVideoFullscreenManagerProxy> create(WebPageProxy&, WebPlaybackSessionManagerProxy&);
    virtual ~WebVideoFullscreenManagerProxy();

    void invalidate();

    void requestHideAndExitFullscreen();
    bool hasMode(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) const;
    bool mayAutomaticallyShowVideoPictureInPicture() const;
    void applicationDidBecomeActive();
    bool isVisible() const;

private:
    friend class WebVideoFullscreenModelContext;

    explicit WebVideoFullscreenManagerProxy(WebPageProxy&, WebPlaybackSessionManagerProxy&);
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;

    typedef std::tuple<RefPtr<WebVideoFullscreenModelContext>, RefPtr<PlatformWebVideoFullscreenInterface>> ModelInterfaceTuple;
    ModelInterfaceTuple createModelAndInterface(uint64_t contextId);
    ModelInterfaceTuple& ensureModelAndInterface(uint64_t contextId);
    WebVideoFullscreenModelContext& ensureModel(uint64_t contextId);
    PlatformWebVideoFullscreenInterface& ensureInterface(uint64_t contextId);
    void addClientForContext(uint64_t contextId);
    void removeClientForContext(uint64_t contextId);

    // Messages from WebVideoFullscreenManager
    void setupFullscreenWithID(uint64_t contextId, uint32_t videoLayerID, const WebCore::IntRect& initialRect, float hostingScaleFactor, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool allowsPictureInPicture);
    void setVideoDimensions(uint64_t contextId, bool hasVideo, unsigned width, unsigned height);
    void enterFullscreen(uint64_t contextId);
    void exitFullscreen(uint64_t contextId, WebCore::IntRect finalRect);
    void cleanupFullscreen(uint64_t contextId);
    void preparedToReturnToInline(uint64_t contextId, bool visible, WebCore::IntRect inlineRect);
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void exitFullscreenWithoutAnimationToMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
#endif

    // Messages to WebVideoFullscreenManager
    void requestFullscreenMode(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void didSetupFullscreen(uint64_t contextId);
    void didExitFullscreen(uint64_t contextId);
    void didEnterFullscreen(uint64_t contextId);
    void didCleanupFullscreen(uint64_t contextId);
    void setVideoLayerFrame(uint64_t contextId, WebCore::FloatRect);
    void setVideoLayerGravity(uint64_t contextId, WebCore::WebVideoFullscreenModel::VideoGravity);
    void fullscreenModeChanged(uint64_t contextId, WebCore::HTMLMediaElementEnums::VideoFullscreenMode);
    void fullscreenMayReturnToInline(uint64_t contextId);

    WebPageProxy* m_page;
    Ref<WebPlaybackSessionManagerProxy> m_playbackSessionManagerProxy;
    HashMap<uint64_t, ModelInterfaceTuple> m_contextMap;
    uint64_t m_controlsManagerContextId { 0 };
    HashMap<uint64_t, int> m_clientCounts;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))

#endif // WebVideoFullscreenManagerProxy_h
