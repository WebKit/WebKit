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

#ifndef TiledDrawingArea_h
#define TiledDrawingArea_h

#if ENABLE(TILED_BACKING_STORE)

#include "DrawingArea.h"
#include "RunLoop.h"
#include <WebCore/IntRect.h>
#include <wtf/Deque.h>

namespace WebKit {

class ShareableBitmap;

class TiledDrawingArea : public DrawingArea {
public:
    explicit TiledDrawingArea(WebPage*);
    virtual ~TiledDrawingArea();

    virtual void setNeedsDisplay(const WebCore::IntRect&);
    virtual void scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollDelta);
    virtual void display();

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachCompositingContext() { }
    virtual void detachCompositingContext() { }
    virtual void setRootCompositingLayer(WebCore::GraphicsLayer*) { }
    virtual void scheduleCompositingLayerSync() { }
    virtual void syncCompositingLayers() { }
#endif

private:
    void scheduleDisplay();

    // CoreIPC message handlers.
    virtual void setSize(const WebCore::IntSize& viewSize);
    virtual void suspendPainting();
    virtual void resumePainting();
    virtual void didUpdate();
    virtual void cancelTileUpdate(int tileID);
    virtual void requestTileUpdate(int tileID, const WebCore::IntRect& dirtyRect, float scale);

    // Platform overrides
    void paintIntoBitmap(ShareableBitmap*, const WebCore::IntRect& tileRect, float scale);

    void scheduleTileUpdate();
    void tileUpdateTimerFired();

    void updateTile(int tileID, const WebCore::IntRect& dirtyRect, float scale);

    WebCore::IntRect m_dirtyRect;
    bool m_isWaitingForUpdate;
    bool m_shouldPaint;
    RunLoop::Timer<TiledDrawingArea> m_displayTimer;

    struct TileUpdate {
        int tileID;
        WebCore::IntRect dirtyRect;
        float scale;
    };
    typedef Deque<OwnPtr<TileUpdate> > UpdateList;
    UpdateList m_pendingUpdates;
    RunLoop::Timer<TiledDrawingArea> m_tileUpdateTimer;
};

} // namespace WebKit

#endif // TILED_BACKING_STORE

#endif // TiledDrawingArea_h
