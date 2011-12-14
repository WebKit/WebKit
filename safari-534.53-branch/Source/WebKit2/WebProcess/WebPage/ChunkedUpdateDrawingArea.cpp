/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ChunkedUpdateDrawingArea.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"

using namespace WebCore;

namespace WebKit {

ChunkedUpdateDrawingArea::ChunkedUpdateDrawingArea(WebPage* webPage)
    : DrawingArea(DrawingAreaTypeChunkedUpdate, webPage)
    , m_isWaitingForUpdate(false)
    , m_paintingIsSuspended(false)
    , m_displayTimer(WebProcess::shared().runLoop(), this, &ChunkedUpdateDrawingArea::display)
{
}

ChunkedUpdateDrawingArea::~ChunkedUpdateDrawingArea()
{
}

void ChunkedUpdateDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    // FIXME: Do something much smarter.
    setNeedsDisplay(scrollRect);
}

void ChunkedUpdateDrawingArea::setNeedsDisplay(const IntRect& rect)
{
    // FIXME: Collect a set of rects/region instead of just the union
    // of all rects.
    m_dirtyRect.unite(rect);
    scheduleDisplay();
}

void ChunkedUpdateDrawingArea::display()
{
    ASSERT(!m_isWaitingForUpdate);
 
    if (m_paintingIsSuspended)
        return;

    if (m_dirtyRect.isEmpty())
        return;

    // Layout if necessary.
    m_webPage->layoutIfNeeded();
 
    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    // Create a new UpdateChunk and paint into it.
    UpdateChunk updateChunk(dirtyRect);
    paintIntoUpdateChunk(&updateChunk);

    WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::Update, m_webPage->pageID(), CoreIPC::In(updateChunk));

    m_isWaitingForUpdate = true;
    m_displayTimer.stop();
}

void ChunkedUpdateDrawingArea::forceRepaint()
{
    m_isWaitingForUpdate = false;
    display();
}

void ChunkedUpdateDrawingArea::scheduleDisplay()
{
    if (m_paintingIsSuspended)
        return;

    if (m_isWaitingForUpdate)
        return;
    
    if (m_dirtyRect.isEmpty())
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void ChunkedUpdateDrawingArea::setSize(const IntSize& viewSize)
{
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    // We don't want to wait for an update until we display.
    m_isWaitingForUpdate = false;
    
    m_webPage->setSize(viewSize);
    m_webPage->layoutIfNeeded();

    if (m_paintingIsSuspended) {
        ASSERT(!m_displayTimer.isActive());

        // Painting is suspended, just send back an empty update chunk.
        WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::DidSetSize, m_webPage->pageID(), CoreIPC::In(UpdateChunk()));
        return;
    }

    // Create a new UpdateChunk and paint into it.
    UpdateChunk updateChunk(IntRect(0, 0, viewSize.width(), viewSize.height()));
    paintIntoUpdateChunk(&updateChunk);

    m_displayTimer.stop();

    WebProcess::shared().connection()->deprecatedSend(DrawingAreaProxyLegacyMessage::DidSetSize, m_webPage->pageID(), CoreIPC::In(updateChunk));
}

void ChunkedUpdateDrawingArea::suspendPainting()
{
    ASSERT(!m_paintingIsSuspended);
    
    m_paintingIsSuspended = true;
    m_displayTimer.stop();
}

void ChunkedUpdateDrawingArea::deprecatedResumePainting(bool forceRepaint)
{
    ASSERT(m_paintingIsSuspended);
    
    m_paintingIsSuspended = false;

    if (forceRepaint) {
        // Just set the dirty rect to the entire page size.
        m_dirtyRect = m_webPage->bounds();
    }

    // Schedule a display.
    scheduleDisplay();
}

void ChunkedUpdateDrawingArea::didUpdate()
{
    m_isWaitingForUpdate = false;

    // Display if needed.
    display();
}

void ChunkedUpdateDrawingArea::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
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

        case DrawingAreaLegacyMessage::ResumePainting: {
            bool forceRepaint;
            if (!arguments->decode(CoreIPC::Out(forceRepaint)))
                return;
            
            deprecatedResumePainting(forceRepaint);
            break;
        }
        case DrawingAreaLegacyMessage::DidUpdate:
            didUpdate();
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

} // namespace WebKit
