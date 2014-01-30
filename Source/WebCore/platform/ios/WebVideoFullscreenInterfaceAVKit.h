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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef WebVideoFullscreenInterfaceAVKit_h
#define WebVideoFullscreenInterfaceAVKit_h

#if PLATFORM(IOS)

#include <WebCore/EventListener.h>
#include <WebCore/HTMLMediaElement.h>
#include <WebCore/PlatformLayer.h>
#include <WebCore/WebVideoFullscreenInterface.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS WebAVPlayerController;
OBJC_CLASS AVPlayerViewController;
OBJC_CLASS UIViewController;
OBJC_CLASS UIWindow;

namespace WebCore {
class WebVideoFullscreenModel;
    
class WebVideoFullscreenInterfaceAVKit
    : public WebVideoFullscreenInterface
    , public RefCounted<WebVideoFullscreenInterfaceAVKit> {
        
    RetainPtr<WebAVPlayerController> m_playerController;
    RetainPtr<AVPlayerViewController> m_playerViewController;
    RetainPtr<UIViewController> m_viewController;
    RetainPtr<UIWindow> m_window;
    WebVideoFullscreenModel* m_videoFullscreenModel;
    
public:
    WebVideoFullscreenInterfaceAVKit();
    virtual ~WebVideoFullscreenInterfaceAVKit() { }
    void setWebVideoFullscreenModel(WebVideoFullscreenModel*);
    
    void setDuration(double) override;
    void setCurrentTime(double currentTime, double anchorTime) override;
    void setRate(bool isPlaying, float playbackRate) override;
    void setVideoDimensions(bool hasVideo, float width, float height) override;
    void setVideoLayer(PlatformLayer*) override;
    void setVideoLayerID(uint32_t videoContextID) override {UNUSED_PARAM(videoContextID);};
    void enterFullscreen() override;
    void enterFullscreen(std::function<void()> completion);
    void exitFullscreen() override;
    void exitFullscreen(std::function<void()> completion);
};

}

#endif

#endif
