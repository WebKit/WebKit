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

#ifndef WebFullScreenManagerProxy_h
#define WebFullScreenManagerProxy_h

#if ENABLE(FULLSCREEN_API)

#include "Connection.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace CoreIPC {
class ArgumentDecoder;
class Connection;
class MessageID;
}

namespace WebCore {
class IntRect;
}

#if PLATFORM(MAC)
OBJC_CLASS WKView;
#endif

namespace WebKit {
    
#if PLATFORM(MAC)
typedef WKView PlatformWebView;
#elif PLATFORM(WIN)
class WebView;
typedef WebView PlatformWebView;
#elif PLATFORM(QT)
typedef QGraphicsWKView PlatformWebView;
#elif PLATFORM(GTK)
class WebView;
typedef WebView PlatformWebView;
#endif

class WebPageProxy;
class LayerTreeContext;

class WebFullScreenManagerProxy : public RefCounted<WebFullScreenManagerProxy> {
public:
    static PassRefPtr<WebFullScreenManagerProxy> create(WebPageProxy*);
    virtual ~WebFullScreenManagerProxy();

    void invalidate();

    void setWebView(PlatformWebView*);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply);

    void willEnterFullScreen();
    void didEnterFullScreen();
    void willExitFullScreen();
    void didExitFullScreen();
    void beginEnterFullScreenAnimation(float duration);
    void beginExitFullScreenAnimation(float duration);
    void disposeOfLayerClient();

private:
    WebFullScreenManagerProxy(WebPageProxy*);

    void supportsFullScreen(bool withKeyboard, bool&);
    void enterFullScreen();
    void exitFullScreen();
    void beganEnterFullScreenAnimation();
    void finishedEnterFullScreenAnimation(bool completed);
    void beganExitFullScreenAnimation();
    void finishedExitFullScreenAnimation(bool completed);
    void enterAcceleratedCompositingMode(const LayerTreeContext&);
    void exitAcceleratedCompositingMode();
    void getFullScreenRect(WebCore::IntRect&);

    WebPageProxy* m_page;
    PlatformWebView* m_webView;

    void didReceiveWebFullScreenManagerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebFullScreenManagerProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder* arguments, CoreIPC::ArgumentEncoder* reply);
};

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)

#endif // WebFullScreenManagerProxy_h
