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
#include "TiledDrawingArea.h"

#if ENABLE(TILED_BACKING_STORE)

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
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

    WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::Invalidate, m_webPage->pageID(), CoreIPC::In(dirtyRect));

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

    WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::DidSetSize, m_webPage->pageID(), CoreIPC::In(viewSize));
}

void TiledDrawingArea::suspendPainting()
{
    ASSERT(m_shouldPaint);

    m_shouldPaint = false;
    m_displayTimer.stop();
}

void TiledDrawingArea::resumePainting()
{
    ASSERT(!m_shouldPaint);

    m_shouldPaint = true;

    // Display if needed.
    display();
}

void TiledDrawingArea::didUpdate()
{
    // Display if needed.
    display();
}

void TiledDrawingArea::updateTile(int tileID, const IntRect& dirtyRect, float scale)
{
    m_webPage->layoutIfNeeded();

    UpdateChunk updateChunk(dirtyRect);
    paintIntoUpdateChunk(&updateChunk, scale);

    unsigned pendingUpdateCount = m_pendingUpdates.size();
    WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::TileUpdated, m_webPage->pageID(), CoreIPC::In(tileID, updateChunk, scale, pendingUpdateCount));
}

void TiledDrawingArea::tileUpdateTimerFired()
{
    ASSERT(!m_pendingUpdates.isEmpty());

    UpdateMap::iterator it = m_pendingUpdates.begin();
    TileUpdate update = it->second;
    m_pendingUpdates.remove(it);

    updateTile(update.tileID, update.dirtyRect, update.scale);

    if (m_pendingUpdates.isEmpty())
        WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::AllTileUpdatesProcessed, m_webPage->pageID(), CoreIPC::In());
    else
        m_tileUpdateTimer.startOneShot(0.001);
}

void TiledDrawingArea::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    switch (messageID.get<DrawingAreaLegacyMessage::Kind>()) {
    case DrawingAreaLegacyMessage::SetSize: {
        IntSize size;
        if (!arguments->decode(CoreIPC::Out(size)))
            return;

        setSize(size);
        break;
    }
    case DrawingAreaLegacyMessage::SuspendPainting:
        suspendPainting();
        break;
    case DrawingAreaLegacyMessage::ResumePainting:
        resumePainting();
        break;
    case DrawingAreaLegacyMessage::CancelTileUpdate: {
        int tileID;
        if (!arguments->decode(CoreIPC::Out(tileID)))
            return;
        UpdateMap::iterator it = m_pendingUpdates.find(tileID);
        if (it != m_pendingUpdates.end()) {
            m_pendingUpdates.remove(it);
            if (m_pendingUpdates.isEmpty()) {
                WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::AllTileUpdatesProcessed, m_webPage->pageID(), CoreIPC::In());
                m_tileUpdateTimer.stop();
            }
        }
        break;
    }
    case DrawingAreaLegacyMessage::RequestTileUpdate: {
        TileUpdate update;
        if (!arguments->decode(CoreIPC::Out(update.tileID, update.dirtyRect, update.scale)))
            return;
        UpdateMap::iterator it = m_pendingUpdates.find(update.tileID);
        if (it != m_pendingUpdates.end())
            it->second.dirtyRect.unite(update.dirtyRect);
        else {
            m_pendingUpdates.add(update.tileID, update);
            if (!m_tileUpdateTimer.isActive())
                m_tileUpdateTimer.startOneShot(0);
        }
        break;
    }
    case DrawingAreaLegacyMessage::TakeSnapshot: {
        IntSize targetSize;
        IntRect contentsRect;

        if (!arguments->decode(CoreIPC::Out(targetSize, contentsRect)))
            return;

        m_webPage->layoutIfNeeded();

        contentsRect.intersect(IntRect(IntPoint::zero(), m_webPage->mainFrame()->coreFrame()->view()->contentsSize()));

        float targetScale = float(targetSize.width()) / contentsRect.width();

        UpdateChunk updateChunk(IntRect(IntPoint(contentsRect.x() * targetScale, contentsRect.y() * targetScale), targetSize));
        paintIntoUpdateChunk(&updateChunk, targetScale);

        WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::SnapshotTaken, m_webPage->pageID(), CoreIPC::In(updateChunk));
        break;
    }
    default:
        ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace WebKit

#endif // TILED_BACKING_STORE
