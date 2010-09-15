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

#if USE(ACCELERATED_COMPOSITING)

#include "LayerBackedDrawingArea.h"

#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/GraphicsLayer.h>

using namespace WebCore;

namespace WebKit {

LayerBackedDrawingArea::LayerBackedDrawingArea(DrawingAreaID identifier, WebPage* webPage)
    : DrawingArea(LayerBackedDrawingAreaType, identifier, webPage)
    , m_syncTimer(WebProcess::shared().runLoop(), this, &LayerBackedDrawingArea::syncCompositingLayers)
#if PLATFORM(MAC) && HAVE(HOSTED_CORE_ANIMATION)
    , m_remoteLayerRef(0)
#endif
    , m_attached(false)
    , m_shouldPaint(true)
{
    m_backingLayer = GraphicsLayer::create(this);
    m_backingLayer->setDrawsContent(true);
#ifndef NDEBUG
    m_backingLayer->setName("DrawingArea backing layer");
#endif
    m_backingLayer->syncCompositingStateForThisLayerOnly();
    
    platformInit();
}

LayerBackedDrawingArea::~LayerBackedDrawingArea()
{
    platformClear();
}

void LayerBackedDrawingArea::invalidateWindow(const IntRect& rect, bool immediate)
{

}

void LayerBackedDrawingArea::invalidateContentsAndWindow(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void LayerBackedDrawingArea::invalidateContentsForSlowScroll(const IntRect& rect, bool immediate)
{
    setNeedsDisplay(rect);
}

void LayerBackedDrawingArea::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
    // FIXME: Do something much smarter.
    setNeedsDisplay(rectToScroll);
}

void LayerBackedDrawingArea::setNeedsDisplay(const IntRect& rect)
{
    m_backingLayer->setNeedsDisplayInRect(rect);
    m_backingLayer->syncCompositingStateForThisLayerOnly();

#if PLATFORM(MAC)
    scheduleUpdateLayoutRunLoopObserver();
#endif
}

void LayerBackedDrawingArea::display()
{
    // Layout if necessary.
    m_webPage->layoutIfNeeded();
}

void LayerBackedDrawingArea::scheduleDisplay()
{
}

void LayerBackedDrawingArea::setSize(const IntSize& viewSize)
{
    ASSERT(m_shouldPaint);
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    m_backingLayer->setSize(viewSize);
    m_backingLayer->syncCompositingStateForThisLayerOnly();
    
    m_webPage->setSize(viewSize);

    // Layout if necessary.
    m_webPage->layoutIfNeeded();

    WebProcess::shared().connection()->send(DrawingAreaProxyMessage::DidSetSize, m_webPage->pageID(), CoreIPC::In());
}

void LayerBackedDrawingArea::suspendPainting()
{
    ASSERT(m_shouldPaint);
    
    m_shouldPaint = false;
}

void LayerBackedDrawingArea::resumePainting()
{
    ASSERT(!m_shouldPaint);
    
    m_shouldPaint = true;
    
    // Display if needed.
    display();
}

void LayerBackedDrawingArea::didUpdate()
{
    // Display if needed.
    display();
}

void LayerBackedDrawingArea::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* arguments)
{
    DrawingAreaID targetDrawingAreaID;
    if (!arguments->decode(CoreIPC::Out(targetDrawingAreaID)))
        return;

    // We can switch drawing areas on the fly, so if this message was targetted at an obsolete drawing area, ignore it.
    if (targetDrawingAreaID != id())
        return;

    switch (messageID.get<DrawingAreaMessage::Kind>()) {
        case DrawingAreaMessage::SetSize: {
            IntSize size;
            if (!arguments->decode(CoreIPC::Out(size)))
                return;

            setSize(size);
            break;
        }
        
        case DrawingAreaMessage::SuspendPainting:
            suspendPainting();
            break;

        case DrawingAreaMessage::ResumePainting:
            resumePainting();
            break;

        case DrawingAreaMessage::DidUpdate:
            didUpdate();
            break;

        default:
            ASSERT_NOT_REACHED();
            break;
    }
}

// GraphicsLayerClient methods
void LayerBackedDrawingArea::paintContents(const GraphicsLayer*, GraphicsContext& graphicsContext, GraphicsLayerPaintingPhase, const IntRect& inClip)
{
    m_webPage->drawRect(graphicsContext, inClip);
}

bool LayerBackedDrawingArea::showDebugBorders() const
{
    // FIXME: get from settings;
    return false;
}

bool LayerBackedDrawingArea::showRepaintCounter() const
{
    // FIXME: get from settings;
    return false;
}

#if !PLATFORM(MAC)
void LayerBackedDrawingArea::attachCompositingContext(GraphicsLayer*)
{
}

void LayerBackedDrawingArea::detachCompositingContext()
{
}

#if !PLATFORM(MAC)
void LayerBackedDrawingArea::setRootCompositingLayer(WebCore::GraphicsLayer*)
{
}
#endif

void LayerBackedDrawingArea::scheduleCompositingLayerSync()
{
}

void LayerBackedDrawingArea::syncCompositingLayers()
{
}

void LayerBackedDrawingArea::platformInit()
{
}

void LayerBackedDrawingArea::platformClear()
{
}
#endif

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
