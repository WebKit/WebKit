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

#ifndef DrawingArea_h
#define DrawingArea_h

#include "DrawingAreaInfo.h"
#include <WebCore/IntRect.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class GraphicsLayer;
}

namespace WebKit {

class WebPage;
struct WebPageCreationParameters;

class DrawingArea {
    WTF_MAKE_NONCOPYABLE(DrawingArea);

public:
    static PassOwnPtr<DrawingArea> create(WebPage*, const WebPageCreationParameters&);
    virtual ~DrawingArea();
    
#if PLATFORM(MAC) || PLATFORM(WIN)
    void didReceiveDrawingAreaMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
#endif

    virtual void setNeedsDisplay(const WebCore::IntRect&) = 0;
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) = 0;

    // FIXME: These should be pure virtual.
    virtual void pageBackgroundTransparencyChanged() { }
    virtual void forceRepaint() { }

    virtual void didInstallPageOverlay() { }
    virtual void didUninstallPageOverlay() { }
    virtual void setPageOverlayNeedsDisplay(const WebCore::IntRect&) { }

#if USE(ACCELERATED_COMPOSITING)
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) = 0;
    virtual void scheduleCompositingLayerSync() = 0;
    virtual void syncCompositingLayers() = 0;
#endif

    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*) = 0;

protected:
    DrawingArea(DrawingAreaType, WebPage*);

    DrawingAreaType m_type;
    WebPage* m_webPage;

private:
    // CoreIPC message handlers.
    // FIXME: These should be pure virtual.
    virtual void updateBackingStoreState(uint64_t backingStoreStateID, bool respondImmediately, const WebCore::IntSize& size, const WebCore::IntSize& scrollOffset) { }
    virtual void didUpdate() { }
    virtual void suspendPainting() { }
    virtual void resumePainting() { }
};

} // namespace WebKit

#endif // DrawingArea_h
