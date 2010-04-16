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

#include "DrawingAreaUpdateChunk.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebCoreTypeArgumentMarshalling.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

DrawingAreaUpdateChunk::DrawingAreaUpdateChunk(WebPage* webPage)
    : DrawingArea(DrawingAreaUpdateChunkType, webPage)
    , m_displayTimer(WebProcess::shared().runLoop(), this, &DrawingArea::display)
{
}

DrawingAreaUpdateChunk::~DrawingAreaUpdateChunk()
{
}

void DrawingAreaUpdateChunk::invalidateWindow(const IntRect& rect, bool immediate)
{
}

void DrawingAreaUpdateChunk::invalidateContentsAndWindow(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void DrawingAreaUpdateChunk::invalidateContentsForSlowScroll(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void DrawingAreaUpdateChunk::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    setNeedsDisplay(rectToScroll);
}

void DrawingAreaUpdateChunk::setNeedsDisplay(const WebCore::IntRect& rect)
{
    m_dirtyRect.unite(rect);
    scheduleDisplay();
}

void DrawingAreaUpdateChunk::paintIntoUpdateChunk(UpdateChunk* updateChunk)
{
    OwnPtr<HDC> hdc(::CreateCompatibleDC(0));

    void* bits;
    BitmapInfo bmp = BitmapInfo::createBottomUp(updateChunk->frame().size());
    OwnPtr<HBITMAP> hbmp(::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, &bits, updateChunk->memory(), 0));

    HBITMAP hbmpOld = static_cast<HBITMAP>(::SelectObject(hdc.get(), hbmp.get()));
    
    GraphicsContext gc(hdc.get());
    gc.save();
    
    // FIXME: Is this white fill needed?
    RECT rect = updateChunk->frame();
    ::FillRect(hdc.get(), &rect, (HBRUSH)::GetStockObject(WHITE_BRUSH));
    gc.translate(-updateChunk->frame().x(), -updateChunk->frame().y());

    m_webPage->drawRect(gc, updateChunk->frame());

    gc.restore();

    // Re-select the old HBITMAP
    ::SelectObject(hdc.get(), hbmpOld);
}

void DrawingAreaUpdateChunk::display()
{
    if (m_dirtyRect.isEmpty())
        return;

    // Layout if necessary.
    m_webPage->layoutIfNeeded();

    IntRect dirtyRect = m_dirtyRect;
    m_dirtyRect = IntRect();

    UpdateChunk updateChunk(dirtyRect);
    paintIntoUpdateChunk(&updateChunk);

    WebProcess::shared().connection()->send(DrawingAreaProxyMessage::Update, m_webPage->pageID(), CoreIPC::In(updateChunk));

    m_displayTimer.stop();
}

void DrawingAreaUpdateChunk::scheduleDisplay()
{
    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void DrawingAreaUpdateChunk::setSize(const WebCore::IntSize& viewSize)
{
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    m_webPage->setSize(viewSize);

    // Layout if necessary.
    m_webPage->layoutIfNeeded();

    // Create a new UpdateChunk and paint into it.
    UpdateChunk updateChunk(IntRect(0, 0, viewSize.width(), viewSize.height()));
    paintIntoUpdateChunk(&updateChunk);

    m_displayTimer.stop();

    WebProcess::shared().connection()->send(DrawingAreaProxyMessage::DidSetFrame, m_webPage->pageID(), CoreIPC::In(viewSize, updateChunk));
}

void DrawingAreaUpdateChunk::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    switch (messageID.get<DrawingAreaMessage::Kind>()) {
        case DrawingAreaMessage::SetFrame: {
            IntSize size;
            if (!arguments.decode(size))
                return;
            setSize(size);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

} // namespace WebKit
