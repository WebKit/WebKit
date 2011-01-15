/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef LayerBackedDrawingAreaProxy_h
#define LayerBackedDrawingAreaProxy_h

#if USE(ACCELERATED_COMPOSITING)

#include "DrawingAreaProxy.h"
#include <WebCore/IntSize.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class CALayer;
@class WKView;
#else
class CALayer;
class WKView;
#endif
#endif

namespace WebKit {

class WebPageProxy;

#if PLATFORM(MAC)
typedef WKView PlatformWebView;
#elif PLATFORM(WIN)
class WebView;
typedef WebView PlatformWebView;
#endif

class LayerBackedDrawingAreaProxy : public DrawingAreaProxy {
public:
    static PassOwnPtr<LayerBackedDrawingAreaProxy> create(PlatformWebView*, WebPageProxy*);
    virtual ~LayerBackedDrawingAreaProxy();

private:
    LayerBackedDrawingAreaProxy(PlatformWebView*, WebPageProxy*);

    WebPageProxy* page();

    // DrawingAreaProxy
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    virtual bool paint(const WebCore::IntRect&, PlatformDrawingContext);
    virtual void sizeDidChange();
    virtual void setPageIsVisible(bool isVisible);

    virtual void attachCompositingContext(uint32_t contextID);
    virtual void detachCompositingContext();
    
    void didSetSize(const WebCore::IntSize&);
    void update();
    
    void platformSetSize();

    bool m_isWaitingForDidSetFrameNotification;
    bool m_isVisible;

    WebCore::IntSize m_lastSetViewSize;

#if PLATFORM(MAC)
    RetainPtr<CALayer> m_compositingRootLayer;
#endif

    PlatformWebView* m_webView;
};
    
} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)

#endif // LayerBackedDrawingAreaProxy_h
