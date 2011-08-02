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
#include "WebCore/Frame.h"
#include "WebCore/FrameView.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"

using namespace WebCore;

namespace WebKit {

TiledDrawingArea::TiledDrawingArea(WebPage* webPage)
    : DrawingArea(DrawingAreaTypeTiled, webPage)
    , m_isWaitingForUpdate(false)
    , m_shouldPaint(true)
    , m_displayTimer(WebProcess::shared().runLoop(), this, &TiledDrawingArea::display)
    , m_tileUpdateTimer(WebProcess::shared().runLoop(), this, &TiledDrawingArea::tileUpdateTimerFired)
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
    // FIXME: Collect a set of rects/region instead of just the union of all rects.
    m_dirtyRect.unite(rect);
    scheduleDisplay();
}

void TiledDrawingArea::display()
{
    if (!m_shouldPaint)
        return;

    if (m_dirtyRect.isEmpty())
        return;

    m_webPage->layoutIfNeeded();

    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    m_webPage->send(Messages::DrawingAreaProxy::Invalidate(dirtyRect));

    m_displayTimer.stop();
}

void TiledDrawingArea::scheduleDisplay()
{
    if (!m_shouldPaint)
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void TiledDrawingArea::setSize(const IntSize& viewSize)
{
    ASSERT(m_shouldPaint);
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    m_webPage->setSize(viewSize);

    scheduleDisplay();

    m_webPage->send(Messages::DrawingAreaProxy::DidSetSize(viewSize));
}

void TiledDrawingArea::suspendPainting()
{
    ASSERT(m_shouldPaint);

    m_shouldPaint = false;
    m_displayTimer.stop();
    m_tileUpdateTimer.stop();
}

void TiledDrawingArea::resumePainting()
{
    ASSERT(!m_shouldPaint);

    m_shouldPaint = true;

    // Display if needed.
    display();
    if (!m_pendingUpdates.isEmpty())
        scheduleTileUpdate();
}

void TiledDrawingArea::didUpdate()
{
    // Display if needed.
    display();
}

void TiledDrawingArea::updateTile(int tileID, const IntRect& dirtyRect, float scale)
{
    m_webPage->layoutIfNeeded();

    UpdateInfo updateInfo;
    updateInfo.updateRectBounds = dirtyRect;
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(dirtyRect.size(), ShareableBitmap::SupportsAlpha);
    bitmap->createHandle(updateInfo.bitmapHandle);
    paintIntoBitmap(bitmap.get(), dirtyRect, scale);

    unsigned pendingUpdateCount = m_pendingUpdates.size();
    m_webPage->send(Messages::DrawingAreaProxy::TileUpdated(tileID, updateInfo, scale, pendingUpdateCount));
}

void TiledDrawingArea::scheduleTileUpdate()
{
    if (!m_shouldPaint)
        return;

    if (m_tileUpdateTimer.isActive())
        return;

    m_tileUpdateTimer.startOneShot(0);
}

void TiledDrawingArea::tileUpdateTimerFired()
{
    ASSERT(!m_pendingUpdates.isEmpty());

    OwnPtr<TileUpdate> update = m_pendingUpdates.first().release();
    m_pendingUpdates.removeFirst();

    updateTile(update->tileID, update->dirtyRect, update->scale);

    if (m_pendingUpdates.isEmpty())
        m_webPage->send(Messages::DrawingAreaProxy::AllTileUpdatesProcessed());
    else
        scheduleTileUpdate();
}

void TiledDrawingArea::cancelTileUpdate(int tileID)
{
    UpdateList::iterator end = m_pendingUpdates.end();
    for (UpdateList::iterator it = m_pendingUpdates.begin(); it != end; ++it) {
        if ((*it)->tileID == tileID) {
            m_pendingUpdates.remove(it);
            break;
        }
    }
    if (m_pendingUpdates.isEmpty()) {
        m_webPage->send(Messages::DrawingAreaProxy::AllTileUpdatesProcessed());
        m_tileUpdateTimer.stop();
    }
}

void TiledDrawingArea::requestTileUpdate(int tileID, const WebCore::IntRect& dirtyRect, float scale)
{
    UpdateList::iterator end = m_pendingUpdates.end();
    for (UpdateList::iterator it = m_pendingUpdates.begin(); it != end; ++it) {
        if ((*it)->tileID == tileID) {
            (*it)->dirtyRect.unite(dirtyRect);
            return;
        }
    }

    OwnPtr<TileUpdate> update(adoptPtr(new TileUpdate));
    update->tileID = tileID;
    update->dirtyRect = dirtyRect;
    update->scale = scale;
    m_pendingUpdates.append(update.release());
    scheduleTileUpdate();
}

} // namespace WebKit

#endif // TILED_BACKING_STORE
