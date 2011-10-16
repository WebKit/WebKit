/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.
 
 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef TiledBackingStore_h
#define TiledBackingStore_h

#if USE(TILED_BACKING_STORE)

#include "FloatPoint.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "Tile.h"
#include "TiledBackingStoreBackend.h"
#include "Timer.h"
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsContext;
class TiledBackingStore;
class TiledBackingStoreClient;

class TiledBackingStore {
    WTF_MAKE_NONCOPYABLE(TiledBackingStore); WTF_MAKE_FAST_ALLOCATED;
public:
    TiledBackingStore(TiledBackingStoreClient*, PassOwnPtr<TiledBackingStoreBackend> = TiledBackingStoreBackend::create());
    ~TiledBackingStore();

    void adjustVisibleRect();
    
    TiledBackingStoreClient* client() { return m_client; }
    float contentsScale() { return m_contentsScale; }
    void setContentsScale(float);
    
    bool contentsFrozen() const { return m_contentsFrozen; }
    void setContentsFrozen(bool);
    void updateTileBuffers();

    void invalidate(const IntRect& dirtyRect);
    void paint(GraphicsContext*, const IntRect&);
    
    IntSize tileSize() { return m_tileSize; }
    void setTileSize(const IntSize&);
    
    double tileCreationDelay() const { return m_tileCreationDelay; }
    void setTileCreationDelay(double delay);
    
    // Tiled are dropped outside the keep area, and created for cover area. The values a relative to the viewport size.
    void getKeepAndCoverAreaMultipliers(float& keepMultiplier, float& coverMultiplier)
    {
        keepMultiplier = m_keepAreaMultiplier;
        coverMultiplier = m_coverAreaMultiplier;
    }
    void setKeepAndCoverAreaMultipliers(float keepMultiplier, float coverMultiplier);
    void setVisibleRectTrajectoryVector(const FloatPoint&);

    IntRect mapToContents(const IntRect&) const;
    IntRect mapFromContents(const IntRect&) const;

    IntRect tileRectForCoordinate(const Tile::Coordinate&) const;
    Tile::Coordinate tileCoordinateForPoint(const IntPoint&) const;
    double tileDistance(const IntRect& viewport, const Tile::Coordinate&) const;
    float coverageRatio(const WebCore::IntRect& contentsRect);

private:
    void startTileBufferUpdateTimer();
    void startTileCreationTimer();
    
    typedef Timer<TiledBackingStore> TileTimer;

    void tileBufferUpdateTimerFired(TileTimer*);
    void tileCreationTimerFired(TileTimer*);
    
    void createTiles();
    IntRect computeKeepRect(const IntRect& visibleRect) const;
    IntRect computeCoverRect(const IntRect& visibleRect) const;
    
    void commitScaleChange();

    bool resizeEdgeTiles();
    void dropTilesOutsideRect(const IntRect&);
    
    PassRefPtr<Tile> tileAt(const Tile::Coordinate&) const;
    void setTile(const Tile::Coordinate& coordinate, PassRefPtr<Tile> tile);
    void removeTile(const Tile::Coordinate& coordinate);

    IntRect contentsRect() const;
    
    void paintCheckerPattern(GraphicsContext*, const IntRect&, const Tile::Coordinate&);

private:
    TiledBackingStoreClient* m_client;
    OwnPtr<TiledBackingStoreBackend> m_backend;

    typedef HashMap<Tile::Coordinate, RefPtr<Tile> > TileMap;
    TileMap m_tiles;

    TileTimer* m_tileBufferUpdateTimer;
    TileTimer* m_tileCreationTimer;

    IntSize m_tileSize;
    double m_tileCreationDelay;
    float m_keepAreaMultiplier;
    float m_coverAreaMultiplier;
    FloatPoint m_visibleRectTrajectoryVector;
    
    IntRect m_previousVisibleRect;
    float m_contentsScale;
    float m_pendingScale;

    bool m_contentsFrozen;

    friend class Tile;
};

}

#endif
#endif
