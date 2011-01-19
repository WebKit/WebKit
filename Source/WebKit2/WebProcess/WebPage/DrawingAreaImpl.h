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

#ifndef DrawingAreaImpl_h
#define DrawingAreaImpl_h

#include "DrawingArea.h"
#include "Region.h"
#include "RunLoop.h"

namespace WebKit {

struct UpdateInfo;

class DrawingAreaImpl : public DrawingArea {
public:
    static PassRefPtr<DrawingAreaImpl> create(WebPage*, const WebPageCreationParameters&);
    virtual ~DrawingAreaImpl();

private:
    DrawingAreaImpl(WebPage*, const WebPageCreationParameters&);

    // DrawingArea
    virtual void setNeedsDisplay(const WebCore::IntRect&);
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset);
    virtual void attachCompositingContext();
    virtual void detachCompositingContext();
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*);
    virtual void scheduleCompositingLayerSync();
    virtual void syncCompositingLayers();
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);

    // CoreIPC message handlers.
    virtual void setSize(const WebCore::IntSize&);
    virtual void didUpdate();
    virtual void suspendPainting();
    virtual void resumePainting();

    void scheduleDisplay();
    void display();
    void display(UpdateInfo&);

    Region m_dirtyRegion;
    WebCore::IntRect m_scrollRect;
    WebCore::IntSize m_scrollOffset;
    
    // Whether we're waiting for a DidUpdate message. Used for throttling paints so that the 
    // web process won't paint more frequent than the UI process can handle.
    bool m_isWaitingForDidUpdate;

    // Whether painting is suspended. We'll still keep track of the dirty region but we 
    // won't paint until painting has resumed again.
    bool m_isPaintingSuspended;

    RunLoop::Timer<DrawingAreaImpl> m_displayTimer;
};

} // namespace WebKit

#endif // DrawingAreaImpl_h
