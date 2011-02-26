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

#if ENABLE(TILED_BACKING_STORE)

#include "DrawingAreaProxy.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <wtf/HashSet.h>

#include "RunLoop.h"
#include "TiledDrawingAreaTile.h"

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class WKView;
#else
class WKView;
#endif
#elif PLATFORM(QT)
class QGraphicsWKView;
#include <QImage>
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

class TiledDrawingAreaProxy : public DrawingAreaProxy {
public:
    static PassOwnPtr<TiledDrawingAreaProxy> create(PlatformWebView* webView, WebPageProxy*);

    TiledDrawingAreaProxy(PlatformWebView*, WebPageProxy*);
    virtual ~TiledDrawingAreaProxy();

    float contentsScale() const { return m_contentsScale; }
    void setContentsScale(float);

    void waitUntilUpdatesComplete();

    void takeSnapshot(const WebCore::IntSize& size, const WebCore::IntRect& contentsRect);

#if USE(ACCELERATED_COMPOSITING)
    virtual void attachCompositingContext(uint32_t /* contextID */) { }
    virtual void detachCompositingContext() { }
#endif

    void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);

    WebCore::IntSize tileSize() { return m_tileSize; }
    void setTileSize(const WebCore::IntSize&);

    double tileCreationDelay() const { return m_tileCreationDelay; }
    void setTileCreationDelay(double delay);

    // Tiled are dropped outside the keep area, and created for cover area. The values a relative to the viewport size.
    void getKeepAndCoverAreaMultipliers(WebCore::FloatSize& keepMultiplier, WebCore::FloatSize& coverMultiplier)
    {
        keepMultiplier = m_keepAreaMultiplier;
        coverMultiplier = m_coverAreaMultiplier;
    }
    void setKeepAndCoverAreaMultipliers(const WebCore::FloatSize& keepMultiplier, const WebCore::FloatSize& coverMultiplier);

    void tileBufferUpdateComplete();

    WebCore::IntRect mapToContents(const WebCore::IntRect&) const;
    WebCore::IntRect mapFromContents(const WebCore::IntRect&) const;

    bool hasPendingUpdates() const;

private:
    WebPageProxy* page();
    WebCore::IntRect webViewVisibleRect();
    void updateWebView(const Vector<WebCore::IntRect>& paintedArea);

    void snapshotTaken(UpdateChunk&);

    // DrawingAreaProxy
    virtual void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    virtual bool paint(const WebCore::IntRect&, PlatformDrawingContext);
    virtual void sizeDidChange();
    virtual void setPageIsVisible(bool isVisible);

    void didSetSize(const WebCore::IntSize&);
    void invalidate(const WebCore::IntRect& rect);
    void adjustVisibleRect();

    void requestTileUpdate(int tileID, const WebCore::IntRect& dirtyRect);

    PassRefPtr<TiledDrawingAreaTile> createTile(const TiledDrawingAreaTile::Coordinate&);

    void startTileBufferUpdateTimer();
    void startTileCreationTimer();

    void tileBufferUpdateTimerFired();
    void tileCreationTimerFired();

    void updateTileBuffers();
    void createTiles();

    bool resizeEdgeTiles();
    void dropTilesOutsideRect(const WebCore::IntRect&);

    PassRefPtr<TiledDrawingAreaTile> tileAt(const TiledDrawingAreaTile::Coordinate&) const;
    void setTile(const TiledDrawingAreaTile::Coordinate& coordinate, RefPtr<TiledDrawingAreaTile> tile);
    void removeTile(const TiledDrawingAreaTile::Coordinate& coordinate);
    void removeAllTiles();

    WebCore::IntRect contentsRect() const;

    WebCore::IntRect calculateKeepRect(const WebCore::IntRect& visibleRect) const;
    WebCore::IntRect calculateCoverRect(const WebCore::IntRect& visibleRect) const;

    WebCore::IntRect tileRectForCoordinate(const TiledDrawingAreaTile::Coordinate&) const;
    TiledDrawingAreaTile::Coordinate tileCoordinateForPoint(const WebCore::IntPoint&) const;
    double tileDistance(const WebCore::IntRect& viewport, const TiledDrawingAreaTile::Coordinate&);

private:
    bool m_isWaitingForDidSetFrameNotification;
    bool m_isVisible;

    WebCore::IntSize m_viewSize; // Size of the BackingStore as well.
    WebCore::IntSize m_lastSetViewSize;

    PlatformWebView* m_webView;

    typedef HashMap<TiledDrawingAreaTile::Coordinate, RefPtr<TiledDrawingAreaTile> > TileMap;
    TileMap m_tiles;

    WTF::HashMap<int, TiledDrawingAreaTile*> m_tilesByID;

    typedef RunLoop::Timer<TiledDrawingAreaProxy> TileTimer;
    TileTimer m_tileBufferUpdateTimer;
    TileTimer m_tileCreationTimer;

    WebCore::IntSize m_tileSize;
    double m_tileCreationDelay;
    WebCore::FloatSize m_keepAreaMultiplier;
    WebCore::FloatSize m_coverAreaMultiplier;

    WebCore::IntRect m_previousVisibleRect;
    float m_contentsScale;

    friend class TiledDrawingAreaTile;
};

} // namespace WebKit

#endif // TILED_BACKING_STORE

#endif // TiledDrawingAreaProxy_h
