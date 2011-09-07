/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TiledDrawingArea.h"

#if ENABLE(TILED_BACKING_STORE)

#include "DrawingAreaProxyMessages.h"
#include "MessageID.h"
#include "UpdateInfo.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"

using namespace WebCore;

namespace WebKit {

TiledDrawingArea::TiledDrawingArea(WebPage* webPage)
    : DrawingArea(DrawingAreaTypeTiled, webPage)
    , m_suspended(false)
    , m_isWaitingForUIProcess(false)
    , m_didSendTileUpdate(false)
    , m_mainBackingStore(adoptPtr(new TiledBackingStore(this, TiledBackingStoreRemoteTileBackend::create(this))))
{
}

TiledDrawingArea::~TiledDrawingArea()
{
}

void TiledDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollDelta)
{
    // FIXME: Do something much smarter.
    setNeedsDisplay(scrollRect);
}

void TiledDrawingArea::setNeedsDisplay(const IntRect& rect)
{
    m_mainBackingStore->invalidate(rect);
}

void TiledDrawingArea::setSize(const IntSize& viewSize)
{
    ASSERT(!m_suspended);
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    m_webPage->setSize(viewSize);
}

void TiledDrawingArea::setVisibleContentRectAndScale(const WebCore::IntRect& visibleContentsRect, float scale)
{
    m_visibleContentRect = visibleContentsRect;

    if (scale != m_mainBackingStore->contentsScale()) {
        // Keep the tiles for the previous scale until enough content is available to be shown on the screen for the new scale.
        // If we already have a previous set of tiles it means that two scale changed happened successively.
        // In that case, make sure that our current main tiles have more content to show than the "previous previous"
        // within the visible rect before replacing it.
        if (!m_previousBackingStore || m_mainBackingStore->coverageRatio(m_visibleContentRect) > m_previousBackingStore->coverageRatio(m_visibleContentRect))
            m_previousBackingStore = m_mainBackingStore.release();

        m_mainBackingStore = adoptPtr(new TiledBackingStore(this, TiledBackingStoreRemoteTileBackend::create(this)));
        m_mainBackingStore->setContentsScale(scale);
    } else
        m_mainBackingStore->adjustVisibleRect();
}

void TiledDrawingArea::renderNextFrame()
{
    m_isWaitingForUIProcess = false;
    m_mainBackingStore->updateTileBuffers();
}

void TiledDrawingArea::suspendPainting()
{
    ASSERT(!m_suspended);

    m_suspended = true;
}

void TiledDrawingArea::resumePainting()
{
    ASSERT(m_suspended);

    m_suspended = false;
    m_mainBackingStore->updateTileBuffers();
}

void TiledDrawingArea::tiledBackingStorePaintBegin()
{
    m_webPage->layoutIfNeeded();
}

void TiledDrawingArea::tiledBackingStorePaint(GraphicsContext* graphicsContext, const IntRect& contentRect)
{
    m_webPage->drawRect(*graphicsContext, contentRect);
}

void TiledDrawingArea::tiledBackingStorePaintEnd(const Vector<IntRect>& paintedArea)
{
    if (m_didSendTileUpdate) {
        // Since we know that all tile updates following a page invalidate will all be rendered
        // in one paint pass for all the tiles, we can send the swap tile message here.
        m_webPage->send(Messages::DrawingAreaProxy::DidRenderFrame());
        m_isWaitingForUIProcess = true;
        m_didSendTileUpdate = false;

        // Make sure that we destroy the previous backing store and remove its tiles only after DidRenderFrame
        // was sent to swap recently created tiles' buffer. Else a frame could be rendered after the previous
        // tiles were removed and before the new tile have their first back buffer swapped.
        if (m_previousBackingStore && m_mainBackingStore->coverageRatio(m_visibleContentRect) >= 1.0f)
            m_previousBackingStore.clear();
    }
}

bool TiledDrawingArea::tiledBackingStoreUpdatesAllowed() const
{
    return !m_suspended && !m_isWaitingForUIProcess;
}

IntRect TiledDrawingArea::tiledBackingStoreContentsRect()
{
    return IntRect(IntPoint::zero(), m_webPage->size());
}

IntRect TiledDrawingArea::tiledBackingStoreVisibleRect()
{
    return m_visibleContentRect;
}

Color TiledDrawingArea::tiledBackingStoreBackgroundColor() const
{
    return Color::transparent;
}

void TiledDrawingArea::createTile(int tileID, const UpdateInfo& updateInfo)
{
    m_webPage->send(Messages::DrawingAreaProxy::CreateTile(tileID, updateInfo));
    m_didSendTileUpdate = true;
}
void TiledDrawingArea::updateTile(int tileID, const UpdateInfo& updateInfo)
{
    m_webPage->send(Messages::DrawingAreaProxy::UpdateTile(tileID, updateInfo));
    m_didSendTileUpdate = true;
}
void TiledDrawingArea::removeTile(int tileID)
{
    m_webPage->send(Messages::DrawingAreaProxy::RemoveTile(tileID));
}

} // namespace WebKit

#endif // TILED_BACKING_STORE
