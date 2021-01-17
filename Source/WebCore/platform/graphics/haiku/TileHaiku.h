/*
 * Copyright (C) 2014 Haiku, inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TileHaiku_h
#define TileHaiku_h

#if USE(TILED_BACKING_STORE) && PLATFORM(HAIKU)

#include "IntPoint.h"
#include "IntRect.h"
#include "Tile.h"

#include <Bitmap.h>
#include <Region.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>


namespace WebCore {

class TiledBackingStore;

class TileHaiku : public Tile {
public:
    static PassRefPtr<Tile> create(TiledBackingStore* backingStore, const Coordinate& tileCoordinate)
    {
        return adoptRef(new TileHaiku(backingStore, tileCoordinate));
    }
    virtual ~TileHaiku();

    virtual bool isDirty() const override;
    virtual void invalidate(const IntRect&) override;
    virtual Vector<IntRect> updateBackBuffer() override;
    virtual void swapBackBufferToFront() override;
    virtual bool isReadyToPaint() const override;
    virtual void paint(GraphicsContext*, const IntRect&) override;

    virtual const Tile::Coordinate& coordinate() const { return m_coordinate; }
    virtual const IntRect& rect() const { return m_rect; }
    virtual void resize(const WebCore::IntSize&) override;

protected:
    TileHaiku(TiledBackingStore*, const Coordinate&);

    TiledBackingStore* m_backingStore;
    Coordinate m_coordinate;
    IntRect m_rect;

    BBitmap m_buffer;
    BRegion m_dirtyRegion;
};

}
#endif
#endif

