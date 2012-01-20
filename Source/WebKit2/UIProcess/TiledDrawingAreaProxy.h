/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef TiledDrawingAreaProxy_h
#define TiledDrawingAreaProxy_h

#if USE(TILED_BACKING_STORE)

#include "DrawingAreaProxy.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/RunLoop.h>
#include <wtf/HashSet.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class WKView;
#else
class WKView;
#endif
#endif

namespace WebCore {
class GraphicsContext;
}

#if PLATFORM(QT)
class QQuickWebPage;
typedef QQuickWebPage PlatformWebView;
#endif

namespace WebKit {

class ShareableBitmap;
class TiledDrawingAreaTileSet;
class WebPageProxy;

#if PLATFORM(MAC)
typedef WKView PlatformWebView;
#elif PLATFORM(WIN)
class WebView;
typedef WebView PlatformWebView;
#endif

class TiledDrawingAreaProxy : public DrawingAreaProxy {
public:
    static PassOwnPtr<TiledDrawingAreaProxy> create(PlatformWebView* webView, WebPageProxy*);

    TiledDrawingAreaProxy(PlatformWebView*, WebPageProxy*);
    virtual ~TiledDrawingAreaProxy();

    void setVisibleContentRectAndScale(const WebCore::IntRect&, float);
    void setVisibleContentRectTrajectoryVector(const WebCore::FloatPoint&);
    void renderNextFrame();

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachCompositingContext(uint32_t /* contextID */) { }
    virtual void detachCompositingContext() { }
#endif

private:
    WebPageProxy* page();
    void updateWebView(const Vector<WebCore::IntRect>& paintedArea);

    // DrawingAreaProxy
    virtual void sizeDidChange();
    virtual void deviceScaleFactorDidChange();

    virtual void createTile(int tileID, const UpdateInfo&);
    virtual void updateTile(int tileID, const UpdateInfo&);
    virtual void didRenderFrame();
    virtual void removeTile(int tileID);


private:
    bool m_isWaitingForDidSetFrameNotification;

    PlatformWebView* m_webView;
#if PLATFORM(QT)
    // Maps tile IDs to node IDs.
    HashMap<int, int> m_tileNodeMap;
#endif
};

} // namespace WebKit

#endif // USE(TILED_BACKING_STORE)

#endif // TiledDrawingAreaProxy_h
