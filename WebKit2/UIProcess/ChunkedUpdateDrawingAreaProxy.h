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

#ifndef DrawingAreaProxyUpdateChunk_h
#define DrawingAreaProxyUpdateChunk_h

#include "DrawingAreaProxy.h"
#include <WebCore/IntSize.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class WKView;
#else
class WKView;
#endif
#elif PLATFORM(QT)
#include <QImage>
class QGraphicsWKView;
#endif

namespace WebKit {

class UpdateChunk;
class WebPageProxy;

#if PLATFORM(MAC)
typedef WKView PlatformWebView;
#elif PLATFORM(WIN)
class WebView;
typedef WebView PlatformWebView;
#elif PLATFORM(QT)
typedef QGraphicsWKView PlatformWebView;
#endif

class ChunkedUpdateDrawingAreaProxy : public DrawingAreaProxy {
public:
    static PassOwnPtr<ChunkedUpdateDrawingAreaProxy> create(PlatformWebView*);

    virtual ~ChunkedUpdateDrawingAreaProxy();

private:
    ChunkedUpdateDrawingAreaProxy(PlatformWebView*);

    WebPageProxy* page();

    // DrawingAreaProxy
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual void paint(const WebCore::IntRect&, PlatformDrawingContext);
    virtual void setSize(const WebCore::IntSize&);
    virtual void setPageIsVisible(bool isVisible);
    
    void ensureBackingStore();
    void invalidateBackingStore();
    void platformPaint(const WebCore::IntRect&, PlatformDrawingContext);
    void drawUpdateChunkIntoBackingStore(UpdateChunk*);
    void didSetSize(UpdateChunk*);
    void update(UpdateChunk*);

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachCompositingContext(uint32_t) { }
    virtual void detachCompositingContext() { }
#endif

    bool m_isWaitingForDidSetFrameNotification;
    bool m_isVisible;
    bool m_forceRepaintWhenResumingPainting;

    WebCore::IntSize m_viewSize; // Size of the BackingStore as well.
    WebCore::IntSize m_lastSetViewSize;

#if PLATFORM(MAC)
    // BackingStore
    RetainPtr<CGContextRef> m_bitmapContext;
#elif PLATFORM(WIN)
    // BackingStore
    OwnPtr<HDC> m_backingStoreDC;
    OwnPtr<HBITMAP> m_backingStoreBitmap;
#elif PLATFORM(QT)
    QImage m_backingStoreImage;
#endif

    PlatformWebView* m_webView;
};
    
} // namespace WebKit

#endif // DrawingAreaProxyUpdateChunk_h
