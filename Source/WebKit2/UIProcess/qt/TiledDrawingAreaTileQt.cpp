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

#include "config.h"
#include "TiledDrawingAreaTile.h"

#if ENABLE(TILED_BACKING_STORE)

#include "GraphicsContext.h"
#include "TiledDrawingAreaProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "UpdateChunk.h"
#include <QApplication>
#include <QObject>
#include <QPainter>

using namespace WebCore;

namespace WebKit {

TiledDrawingAreaTile::TiledDrawingAreaTile(TiledDrawingAreaProxy* proxy, const Coordinate& tileCoordinate)
    : m_proxy(proxy)
    , m_coordinate(tileCoordinate)
    , m_rect(proxy->tileRectForCoordinate(tileCoordinate))
    , m_hasUpdatePending(false)
    , m_dirtyRegion(m_rect)
{
    static int id = 0;
    m_ID = ++id;
#ifdef TILE_DEBUG_LOG
    qDebug() << "deleting tile id=" << m_ID;
#endif
}

TiledDrawingAreaTile::~TiledDrawingAreaTile()
{
#ifdef TILE_DEBUG_LOG
    qDebug() << "deleting tile id=" << m_ID;
#endif
}

bool TiledDrawingAreaTile::isDirty() const
{
    return !m_dirtyRegion.isEmpty();
}

bool TiledDrawingAreaTile::isReadyToPaint() const
{
    return !m_buffer.isNull();
}

bool TiledDrawingAreaTile::hasReadyBackBuffer() const
{
    return !m_backBuffer.isNull() && !m_hasUpdatePending;
}

void TiledDrawingAreaTile::invalidate(const IntRect& dirtyRect)
{
    IntRect tileDirtyRect = intersection(dirtyRect, m_rect);
    if (tileDirtyRect.isEmpty())
        return;

    m_dirtyRegion += tileDirtyRect;
}

void TiledDrawingAreaTile::resize(const IntSize& newSize)
{
    IntRect oldRect = m_rect;
    m_rect = IntRect(m_rect.location(), newSize);
    if (m_rect.maxX() > oldRect.maxX())
        invalidate(IntRect(oldRect.maxX(), oldRect.y(), m_rect.maxX() - oldRect.maxX(), m_rect.height()));
    if (m_rect.maxY() > oldRect.maxY())
        invalidate(IntRect(oldRect.x(), oldRect.maxY(), m_rect.width(), m_rect.maxY() - oldRect.maxY()));
}

void TiledDrawingAreaTile::swapBackBufferToFront()
{
    ASSERT(!m_backBuffer.isNull());

    m_buffer = m_backBuffer;
    m_backBuffer = QPixmap();
}

void TiledDrawingAreaTile::paint(GraphicsContext* context, const IntRect& rect)
{
    ASSERT(!m_buffer.isNull());

    IntRect target = intersection(rect, m_rect);
    IntRect source((target.x() - m_rect.x()),
                   (target.y() - m_rect.y()),
                   target.width(),
                   target.height());

    context->platformContext()->drawPixmap(target, m_buffer, source);
}

void TiledDrawingAreaTile::updateFromChunk(UpdateChunk* updateChunk, float)
{
    QImage image(updateChunk->createImage());
    const IntRect& updateChunkRect = updateChunk->rect();

#ifdef TILE_DEBUG_LOG
    qDebug() << "tile updated id=" << ID() << " rect=" << QRect(updateChunkRect);
#endif
    if (updateChunkRect.size() == m_proxy->tileSize()) {
        // Make a deep copy of the image since it's in shared memory.
        m_backBuffer = QPixmap::fromImage(image.copy());
    } else {
        if (m_backBuffer.isNull())
            m_backBuffer = m_buffer.isNull() ? QPixmap(m_proxy->tileSize()) : m_buffer;
        QPainter painter(&m_backBuffer);
        IntSize drawPoint = updateChunkRect.location() - m_rect.location();
        painter.drawImage(QPoint(drawPoint.width(), drawPoint.height()), image);
    }
    m_hasUpdatePending = false;
}

void TiledDrawingAreaTile::updateBackBuffer()
{
    if (isReadyToPaint() && !isDirty())
        return;

    // FIXME: individual rects
    IntRect dirtyRect = m_dirtyRegion.boundingRect();
    m_dirtyRegion = QRegion();

#ifdef TILE_DEBUG_LOG
    qDebug() << "requesting tile update id=" << m_ID << " rect=" << QRect(dirtyRect) << " scale=" << m_proxy->contentsScale();
#endif
    if (!m_proxy->page()->process()->isValid())
        return;
    m_proxy->requestTileUpdate(m_ID, dirtyRect);

    m_hasUpdatePending = true;
}

}
#endif
