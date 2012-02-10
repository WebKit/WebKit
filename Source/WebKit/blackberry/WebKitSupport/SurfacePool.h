/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef SurfacePool_h
#define SurfacePool_h

#include "BackingStoreTile.h"

#include "PlatformContextSkia.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformPrimitives.h>
#include <wtf/Vector.h>

#define ENABLE_COMPOSITING_SURFACE 1

namespace BlackBerry {
namespace WebKit {

class BackingStoreCompositingSurface;

class SurfacePool {
public:
    static SurfacePool* globalSurfacePool();

    void initialize(const BlackBerry::Platform::IntSize&);

    int isActive() const { return !m_tilePool.isEmpty() && !m_buffersSuspended; }
    int isEmpty() const { return m_tilePool.isEmpty(); }
    int size() const { return m_tilePool.size(); }

    typedef WTF::Vector<BackingStoreTile*> TileList;
    const TileList tileList() const { return m_tilePool; }

    PlatformGraphicsContext* createPlatformGraphicsContext(BlackBerry::Platform::Graphics::Drawable*) const;
    PlatformGraphicsContext* lockTileRenderingSurface() const;
    void releaseTileRenderingSurface(PlatformGraphicsContext*) const;
    BackingStoreTile* visibleTileBuffer() const { return m_visibleTileBuffer; }

    void initializeVisibleTileBuffer(const BlackBerry::Platform::IntSize&);

    // This is a shared back buffer that is used by all the tiles since
    // only one tile will be rendering it at a time and we invalidate
    // the whole tile every time we render by copying from the front
    // buffer those portions that we don't render. This allows us to
    // have N+1 tilebuffers rather than N*2 for our double buffered
    // backingstore.
    TileBuffer* backBuffer() const;

    BackingStoreCompositingSurface* compositingSurface() const;

    void notifyScreenRotated();

    std::string sharedPixmapGroup() const;

    void releaseBuffers();
    void createBuffers();

private:
    // This is necessary so BackingStoreTile can atomically swap buffers with m_backBuffer.
    friend class BackingStoreTile;

    SurfacePool();

    TileList m_tilePool;
    BackingStoreTile* m_visibleTileBuffer;
#if USE(ACCELERATED_COMPOSITING)
    RefPtr<BackingStoreCompositingSurface> m_compositingSurface;
#endif
    BlackBerry::Platform::Graphics::Buffer* m_tileRenderingSurface;
    unsigned m_backBuffer;
    bool m_initialized; // SurfacePool has been set up, with or without buffers.
    bool m_buffersSuspended; // Buffer objects exist, but pixel memory has been freed.
};
}
}

#endif // SurfacePool_h
