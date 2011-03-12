/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef WebFullScreenManagerMac_h
#define WebFullScreenManagerMac_h

#if ENABLE(FULLSCREEN_API)

#import "LayerTreeContext.h"
#import "WebFullScreenManager.h"

#import <WebCore/IntRect.h>
#import <wtf/RetainPtr.h>

typedef struct __WKCARemoteLayerClientRef* WKCARemoteLayerClientRef;
OBJC_CLASS WebFullScreenManagerAnimationListener;

namespace WebKit {

class WebFullScreenManagerMac : public WebFullScreenManager {
public:
    static PassRefPtr<WebFullScreenManagerMac> create(WebPage*);

    virtual void setRootFullScreenLayer(WebCore::GraphicsLayer*);

private:
    WebFullScreenManagerMac(WebPage*);
    virtual ~WebFullScreenManagerMac();

    virtual void beginEnterFullScreenAnimation(float duration);
    virtual void beginExitFullScreenAnimation(float duration);

    OwnPtr<WebCore::GraphicsLayer> m_rootLayer;
    WebCore::GraphicsLayer* m_fullScreenRootLayer;
    LayerTreeContext m_layerTreeContext;
    RetainPtr<WKCARemoteLayerClientRef> m_remoteLayerClient;
    RetainPtr<id> m_enterFullScreenListener;
    RetainPtr<id> m_exitFullScreenListener;
};

}

#endif // ENABLE(FULLSCREEN_API)

#endif // WebFullScreenManagerMac_h
