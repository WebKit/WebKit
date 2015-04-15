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

#if PLATFORM(IOS)

#include "MessageReceiver.h"
#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/PlatformCALayer.h>
#include <WebCore/WebVideoFullscreenInterface.h>
#include <WebCore/WebVideoFullscreenModelVideoElement.h>
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

class WebVideoFullscreenManager : public WebCore::WebVideoFullscreenModelVideoElement, public WebCore::WebVideoFullscreenInterface, private IPC::MessageReceiver {
public:
    static PassRefPtr<WebVideoFullscreenManager> create(PassRefPtr<WebPage>);
    virtual ~WebVideoFullscreenManager();
    
    void didReceiveMessage(IPC::Connection&, IPC::MessageDecoder&) override;
    
    bool supportsVideoFullscreen() const;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElement::VideoFullscreenMode);
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&);
    
protected:
    explicit WebVideoFullscreenManager(PassRefPtr<WebPage>);
    virtual bool operator==(const EventListener& rhs) override { return static_cast<WebCore::EventListener*>(this) == &rhs; }
    
    // FullscreenInterface
    virtual void resetMediaState() override;
    virtual void setDuration(double) override;
    virtual void setCurrentTime(double currentTime, double anchorTime) override;
    virtual void setBufferedTime(double bufferedTime) override;
    virtual void setRate(bool isPlaying, float playbackRate) override;
    virtual void setVideoDimensions(bool hasVideo, float width, float height) override;
    virtual void setSeekableRanges(const WebCore::TimeRanges&) override;
    virtual void setCanPlayFastReverse(bool value) override;

    virtual void setAudioMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex) override;
    virtual void setLegibleMediaSelectionOptions(const Vector<String>& options, uint64_t selectedIndex) override;
    virtual void setExternalPlayback(bool enabled, WebVideoFullscreenInterface::ExternalPlaybackTargetType, String localizedDeviceName) override;

    // additional incoming
    virtual void didSetupFullscreen();
    virtual void didEnterFullscreen();
    virtual void didExitFullscreen();
    virtual void didCleanupFullscreen();
    virtual void setVideoLayerGravityEnum(unsigned);
    virtual void fullscreenMayReturnToInline(bool isPageVisible);
    void setVideoLayerFrameFenced(WebCore::FloatRect bounds, IPC::Attachment fencePort);
    
    WebPage* m_page;
    RefPtr<WebCore::HTMLVideoElement> m_videoElement;
    std::unique_ptr<LayerHostingContext> m_layerHostingContext;
    
    bool m_isAnimating;
    bool m_targetIsFullscreen;
    WebCore::HTMLMediaElement::VideoFullscreenMode m_fullscreenMode;
    bool m_isFullscreen;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS)

#endif // WebVideoFullscreenManager_h
