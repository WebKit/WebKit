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

#ifndef TiledDrawingAreaTile_h
#define TiledDrawingAreaTile_h

#include "IntPoint.h"
#include "IntPointHash.h"
#include "IntRect.h"
#include "GraphicsContext.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

#if PLATFORM(QT)
#include <QPixmap>
#include <QRegion>
#endif

namespace WebKit {

class TiledDrawingAreaProxy;
class UpdateChunk;

class TiledDrawingAreaTile : public RefCounted<TiledDrawingAreaTile> {
public:
    typedef WebCore::IntPoint Coordinate;

    static PassRefPtr<TiledDrawingAreaTile> create(TiledDrawingAreaProxy* proxy, const Coordinate& tileCoordinate) { return adoptRef(new TiledDrawingAreaTile(proxy, tileCoordinate)); }
    ~TiledDrawingAreaTile();

    bool isDirty() const;
    void invalidate(const WebCore::IntRect&);
    void updateBackBuffer();
    bool hasReadyBackBuffer() const;
    void swapBackBufferToFront();
    bool isReadyToPaint() const;
    void paint(WebCore::GraphicsContext*, const WebCore::IntRect&);
    bool hasBackBufferUpdatePending() const { return m_hasUpdatePending; }

    const TiledDrawingAreaTile::Coordinate& coordinate() const { return m_coordinate; }
    const WebCore::IntRect& rect() const { return m_rect; }
    void resize(const WebCore::IntSize&);

    void updateFromChunk(UpdateChunk* updateChunk, float);

    int ID() const { return m_ID; }

private:
    TiledDrawingAreaTile(TiledDrawingAreaProxy* proxy, const TiledDrawingAreaTile::Coordinate& tileCoordinate);

    TiledDrawingAreaProxy* m_proxy;
    Coordinate m_coordinate;
    WebCore::IntRect m_rect;

    int m_ID;
    bool m_hasUpdatePending;

#if PLATFORM(QT)
    QPixmap m_buffer;
    QPixmap m_backBuffer;
    QRegion m_dirtyRegion;
#endif
};

}

#endif
