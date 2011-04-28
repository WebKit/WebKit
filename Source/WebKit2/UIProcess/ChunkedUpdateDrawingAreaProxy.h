/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L
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

#ifndef ChunkedUpdateDrawingAreaProxy_h
#define ChunkedUpdateDrawingAreaProxy_h

#include "DrawingAreaProxy.h"
#include <WebCore/IntSize.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/OwnPtr.h>

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
OBJC_CLASS WKView;
#elif PLATFORM(QT)
#include <QImage>
class QGraphicsWKView;
#elif PLATFORM(GTK)
typedef struct _cairo_surface cairo_surface_t;
typedef struct _WebKitWebViewBase WebKitWebViewBase;
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
#elif PLATFORM(GTK)
typedef WebKitWebViewBase PlatformWebView;
#endif

class ChunkedUpdateDrawingAreaProxy : public DrawingAreaProxy {
public:
    static PassOwnPtr<ChunkedUpdateDrawingAreaProxy> create(PlatformWebView*, WebPageProxy*);

    virtual ~ChunkedUpdateDrawingAreaProxy();

private:
    ChunkedUpdateDrawingAreaProxy(PlatformWebView*, WebPageProxy*);

    WebPageProxy* page();

    // DrawingAreaProxy
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual bool paint(const WebCore::IntRect&, PlatformDrawingContext);
    virtual void sizeDidChange();
    virtual void setPageIsVisible(bool isVisible);
    
    void ensureBackingStore();
    void invalidateBackingStore();
    bool platformPaint(const WebCore::IntRect&, PlatformDrawingContext);
    void drawUpdateChunkIntoBackingStore(UpdateChunk*);
    void didSetSize(UpdateChunk*);
    void deprecatedUpdate(UpdateChunk*);

    void sendSetSize();

    bool m_isWaitingForDidSetFrameNotification;
    bool m_isVisible;
    bool m_forceRepaintWhenResumingPainting;

#if PLATFORM(MAC)
    // BackingStore
    RetainPtr<CGContextRef> m_bitmapContext;
#elif PLATFORM(WIN)
    // BackingStore
    OwnPtr<HDC> m_backingStoreDC;
    OwnPtr<HBITMAP> m_backingStoreBitmap;
#elif PLATFORM(QT)
    QImage m_backingStoreImage;
#elif PLATFORM(GTK)
    cairo_surface_t* m_backingStoreImage;
#endif

    PlatformWebView* m_webView;
};
    
} // namespace WebKit

#endif // ChunkedUpdateDrawingAreaProxy_h
