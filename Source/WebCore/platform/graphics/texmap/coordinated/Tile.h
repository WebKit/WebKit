/*
 * Copyright (C) 2010-2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef Tile_h
#define Tile_h

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedSurface.h"
#include "IntPoint.h"
#include "IntPointHash.h"
#include "IntRect.h"

namespace WebCore {

class GraphicsContext;
class TiledBackingStore;

class Tile : public CoordinatedSurface::Client {
public:
    typedef IntPoint Coordinate;

    Tile(TiledBackingStore&, const Coordinate&);
    ~Tile();

    bool isDirty() const;
    void invalidate(const IntRect&);
    bool updateBackBuffer();
    bool isReadyToPaint() const;

    const Coordinate& coordinate() const { return m_coordinate; }
    const IntRect& rect() const { return m_rect; }
    void resize(const IntSize&);

    void paintToSurfaceContext(GraphicsContext&) override;

private:
    TiledBackingStore& m_tiledBackingStore;
    Coordinate m_coordinate;
    IntRect m_rect;

    uint32_t m_ID;
    IntRect m_dirtyRect;
};

} // namespace WebCore

#endif // USE(COORDINATED_GRAPHICS)

#endif // Tile_h
