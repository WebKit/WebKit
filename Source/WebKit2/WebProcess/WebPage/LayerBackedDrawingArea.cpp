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
#include <WebCore/Page.h>
#include <WebCore/Settings.h>

using namespace WebCore;

namespace WebKit {

LayerBackedDrawingArea::LayerBackedDrawingArea(DrawingAreaInfo::Identifier identifier, WebPage* webPage)
    : DrawingArea(DrawingAreaInfo::LayerBacked, identifier, webPage)
    , m_syncTimer(WebProcess::shared().runLoop(), this, &LayerBackedDrawingArea::syncCompositingLayers)
    , m_attached(false)
    , m_shouldPaint(true)
{
    m_hostingLayer = GraphicsLayer::create(this);
    m_hostingLayer->setDrawsContent(false);
#ifndef NDEBUG
    m_hostingLayer->setName("DrawingArea hosting layer");
#endif
    m_hostingLayer->setSize(webPage->size());
    m_backingLayer = GraphicsLayer::create(this);
    m_backingLayer->setDrawsContent(true);
    m_backingLayer->setContentsOpaque(webPage->drawsBackground() && !webPage->drawsTransparentBackground());

#ifndef NDEBUG
    m_backingLayer->setName("DrawingArea backing layer");
#endif
    m_backingLayer->setSize(webPage->size());
    m_hostingLayer->addChild(m_backingLayer.get());
    platformInit();
}

LayerBackedDrawingArea::~LayerBackedDrawingArea()
{
    platformClear();
}

void LayerBackedDrawingArea::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    // FIXME: Do something much smarter.
    setNeedsDisplay(scrollRect);
}

void LayerBackedDrawingArea::setNeedsDisplay(const IntRect& rect)
{
    m_backingLayer->setNeedsDisplayInRect(rect);
    scheduleCompositingLayerSync();
}

void LayerBackedDrawingArea::display()
{
    // Laying out the page can cause the drawing area to change so we keep an extra reference.
    RefPtr<LayerBackedDrawingArea> protect(this);

    // Layout if necessary.
    m_webPage->layoutIfNeeded();

    if (m_webPage->drawingArea() != this)
        return;
}

void LayerBackedDrawingArea::pageBackgroundTransparencyChanged()
{
    m_backingLayer->setContentsOpaque(m_webPage->drawsBackground() && !m_webPage->drawsTransparentBackground());
}

void LayerBackedDrawingArea::scheduleDisplay()
{
}

void LayerBackedDrawingArea::setSize(const IntSize& viewSize)
{
    ASSERT(m_shouldPaint);
    ASSERT_ARG(viewSize, !viewSize.isEmpty());

    m_hostingLayer->setSize(viewSize);
    m_backingLayer->setSize(viewSize);
    scheduleCompositingLayerSync();

    // Laying out the page can cause the drawing area to change so we keep an extra reference.
    RefPtr<LayerBackedDrawingArea> protect(this);
    
    m_webPage->setSize(viewSize);
    m_webPage->layoutIfNeeded();

    if (m_webPage->drawingArea() != this)
        return;
    
    WebProcess::shared().connection()->send(DrawingAreaProxyLegacyMessage::DidSetSize, m_webPage->pageID(), CoreIPC::In(viewSize));
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
    DrawingAreaInfo::Identifier targetIdentifier;
    if (!arguments->decode(CoreIPC::Out(targetIdentifier)))
        return;

    // We can switch drawing areas on the fly, so if this message was targetted at an obsolete drawing area, ignore it.
    if (targetIdentifier != info().identifier)
        return;

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

        case DrawingAreaLegacyMessage::DidUpdate:
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
    return m_webPage->corePage()->settings()->showDebugBorders();
}

bool LayerBackedDrawingArea::showRepaintCounter() const
{
    return m_webPage->corePage()->settings()->showRepaintCounter();
}

#if !PLATFORM(MAC) && !PLATFORM(WIN)
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
