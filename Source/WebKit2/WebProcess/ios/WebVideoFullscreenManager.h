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
#include <WebCore/PlatformCALayer.h>
#include <WebCore/WebVideoFullscreenInterface.h>
#include <WebCore/WebVideoFullscreenModelMediaElement.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace IPC {
class Connection;
class MessageDecoder;
class MessageReceiver;
}

namespace WebCore {
class Node;
}

namespace WebKit {

class WebPage;
class RemoteLayerTreeTransaction;

class WebVideoFullscreenManager : public WebCore::WebVideoFullscreenModelMediaElement, public WebCore::WebVideoFullscreenInterface, private IPC::MessageReceiver {
public:
    static PassRefPtr<WebVideoFullscreenManager> create(PassRefPtr<WebPage>);
    virtual ~WebVideoFullscreenManager();
    
    void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&);
    
    void willCommitLayerTree(RemoteLayerTreeTransaction&);

    bool supportsFullscreen(const WebCore::Node*) const;
    void enterFullscreenForNode(WebCore::Node*);
    void exitFullscreenForNode(WebCore::Node*);
    
protected:
    explicit WebVideoFullscreenManager(PassRefPtr<WebPage>);
    virtual bool operator==(const EventListener& rhs) override { return static_cast<WebCore::EventListener*>(this) == &rhs; }
    
    // FullscreenInterface
    virtual void setDuration(double) override;
    virtual void setCurrentTime(double currentTime, double anchorTime) override;
    virtual void setRate(bool isPlaying, float playbackRate) override;
    virtual void setVideoDimensions(bool hasVideo, float width, float height) override;
    virtual void willLendVideoLayer(PlatformLayer*) override;
    virtual void didLendVideoLayer() override;

    // forward to interface
    virtual void enterFullscreen();
    virtual void exitFullscreen();
    
    // additional incoming
    virtual void didEnterFullscreen();
    virtual void didExitFullscreen();
    
    WebPage* m_page;
    RefPtr<WebCore::Node> m_node;
    RefPtr<WebCore::PlatformCALayer> m_platformCALayer;
    bool m_sendUnparentVideoLayerTransaction;
    
    bool m_isAnimating;
    bool m_targetIsFullscreen;
    bool m_isFullscreen;
};
    
} // namespace WebKit

#endif // PLATFORM(IOS)

#endif // WebVideoFullscreenManager_h
