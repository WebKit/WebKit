/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef TiledBackingStoreRemoteTile_h
#define TiledBackingStoreRemoteTile_h

#if USE(TILED_BACKING_STORE)

#include "ShareableSurface.h"
#include "Tile.h"
#include "TiledBackingStore.h"
#include "WebCore/IntRect.h"

namespace WebCore {
class ImageBuffer;
class TiledBackingStore;
}

namespace WebKit {

class TiledBackingStoreRemoteTileClient;
class SurfaceUpdateInfo;

class TiledBackingStoreRemoteTile : public WebCore::Tile {
public:
    static PassRefPtr<Tile> create(TiledBackingStoreRemoteTileClient* client, WebCore::TiledBackingStore* tiledBackingStore, const Coordinate& tileCoordinate) { return adoptRef(new TiledBackingStoreRemoteTile(client, tiledBackingStore, tileCoordinate)); }
    ~TiledBackingStoreRemoteTile();

    bool isDirty() const;
    void invalidate(const WebCore::IntRect&);
    Vector<WebCore::IntRect> updateBackBuffer();
    void swapBackBufferToFront();
    bool isReadyToPaint() const;
    void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);

    const Coordinate& coordinate() const { return m_coordinate; }
    const WebCore::IntRect& rect() const { return m_rect; }
    void resize(const WebCore::IntSize&);

private:
    TiledBackingStoreRemoteTile(TiledBackingStoreRemoteTileClient*, WebCore::TiledBackingStore*, const Coordinate&);

    TiledBackingStoreRemoteTileClient* m_client;
    WebCore::TiledBackingStore* m_tiledBackingStore;
    Coordinate m_coordinate;
    WebCore::IntRect m_rect;

    int m_ID;
    WebCore::IntRect m_dirtyRect;

    OwnPtr<WebCore::ImageBuffer> m_localBuffer;
};

class TiledBackingStoreRemoteTileClient {
public:
    virtual ~TiledBackingStoreRemoteTileClient() { }
    virtual void createTile(int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void updateTile(int tileID, const SurfaceUpdateInfo&, const WebCore::IntRect&) = 0;
    virtual void removeTile(int tileID) = 0;
    virtual PassOwnPtr<WebCore::GraphicsContext> beginContentUpdate(const WebCore::IntSize&, ShareableSurface::Handle&, WebCore::IntPoint&) = 0;
};

class TiledBackingStoreRemoteTileBackend : public WebCore::TiledBackingStoreBackend {
public:
    static PassOwnPtr<WebCore::TiledBackingStoreBackend> create(TiledBackingStoreRemoteTileClient* client) { return adoptPtr(new TiledBackingStoreRemoteTileBackend(client)); }
    PassRefPtr<WebCore::Tile> createTile(WebCore::TiledBackingStore*, const WebCore::Tile::Coordinate&);
    void paintCheckerPattern(WebCore::GraphicsContext*, const WebCore::FloatRect&);

private:
    TiledBackingStoreRemoteTileBackend(TiledBackingStoreRemoteTileClient*);
    TiledBackingStoreRemoteTileClient* m_client;
};


} // namespace WebKit

#endif // USE(TILED_BACKING_STORE)

#endif // TiledBackingStoreRemoteTile
