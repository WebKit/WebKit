/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "DrawingAreaImpl.h"

#include "DrawingAreaProxyMessages.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>

#ifndef __APPLE__
#error "This drawing area is not ready for use by other ports yet."
#endif

using namespace WebCore;

namespace WebKit {

PassRefPtr<DrawingAreaImpl> DrawingAreaImpl::create(WebPage* webPage, const WebPageCreationParameters& parameters)
{
    return adoptRef(new DrawingAreaImpl(webPage, parameters));
}

DrawingAreaImpl::~DrawingAreaImpl()
{
}

DrawingAreaImpl::DrawingAreaImpl(WebPage* webPage, const WebPageCreationParameters& parameters)
    : DrawingArea(DrawingAreaInfo::Impl, parameters.drawingAreaInfo.identifier, webPage)
    , m_isWaitingForDidUpdate(false)
    , m_isPaintingSuspended(!parameters.isVisible)
    , m_displayTimer(WebProcess::shared().runLoop(), this, &DrawingAreaImpl::display)
{
}

void DrawingAreaImpl::setNeedsDisplay(const IntRect& rect)
{
    if (rect.isEmpty())
        return;

    m_dirtyRegion.unite(rect);
    scheduleDisplay();
}

void DrawingAreaImpl::scroll(const IntRect& scrollRect, const IntSize& scrollOffset)
{
    if (!m_scrollRect.isEmpty() && scrollRect != m_scrollRect) {
        unsigned scrollArea = scrollRect.width() * scrollRect.height();
        unsigned currentScrollArea = m_scrollRect.width() * m_scrollRect.height();

        if (currentScrollArea >= scrollArea) {
            // The rect being scrolled is at least as large as the rect we'd like to scroll.
            // Go ahead and just invalidate the scroll rect.
            setNeedsDisplay(scrollRect);
            return;
        }

        // Just repaint the entire current scroll rect, we'll scroll the new rect instead.
        setNeedsDisplay(m_scrollRect);
        m_scrollRect = IntRect();
        m_scrollOffset = IntSize();
    }

    // Get the part of the dirty region that is in the scroll rect.
    Region dirtyRegionInScrollRect = intersect(scrollRect, m_dirtyRegion);
    if (!dirtyRegionInScrollRect.isEmpty()) {
        // There are parts of the dirty region that are inside the scroll rect.
        // We need to subtract them from the region, move them and re-add them.
        m_dirtyRegion.subtract(scrollRect);

        // Move the dirty parts.
        Region movedDirtyRegionInScrollRect = intersect(translate(dirtyRegionInScrollRect, scrollOffset), scrollRect);

        // And add them back.
        m_dirtyRegion.unite(movedDirtyRegionInScrollRect);
    } 
    
    // Compute the scroll repaint region.
    Region scrollRepaintRegion = subtract(scrollRect, translate(scrollRect, scrollOffset));

    m_dirtyRegion.unite(scrollRepaintRegion);

    m_scrollRect = scrollRect;
    m_scrollOffset += scrollOffset;
}

void DrawingAreaImpl::attachCompositingContext()
{
}

void DrawingAreaImpl::detachCompositingContext()
{
}

void DrawingAreaImpl::setRootCompositingLayer(WebCore::GraphicsLayer*)
{
}

void DrawingAreaImpl::scheduleCompositingLayerSync()
{
}

void DrawingAreaImpl::syncCompositingLayers()
{
}

void DrawingAreaImpl::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*)
{
}

void DrawingAreaImpl::setSize(const IntSize& size)
{
    // Set this to false since we're about to call display().
    m_isWaitingForDidUpdate = false;

    m_webPage->setSize(size);
    m_webPage->layoutIfNeeded();

    UpdateInfo updateInfo;

    if (m_isPaintingSuspended) {
        updateInfo.timestamp = currentTime();
        updateInfo.viewSize = m_webPage->size();
    } else
        display(updateInfo);

    m_webPage->send(Messages::DrawingAreaProxy::DidSetSize(updateInfo));
}

void DrawingAreaImpl::didUpdate()
{
    m_isWaitingForDidUpdate = false;

    // Display if needed.
    display();
}

void DrawingAreaImpl::suspendPainting()
{
    ASSERT(!m_isPaintingSuspended);

    m_isPaintingSuspended = true;
    m_displayTimer.stop();
}

void DrawingAreaImpl::resumePainting()
{
    ASSERT(m_isPaintingSuspended);

    m_isPaintingSuspended = false;

    // FIXME: Repaint if needed.
}

void DrawingAreaImpl::scheduleDisplay()
{
    if (m_isWaitingForDidUpdate)
        return;

    if (m_isPaintingSuspended)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void DrawingAreaImpl::display()
{
    ASSERT(!m_isWaitingForDidUpdate);

    if (m_isPaintingSuspended)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    UpdateInfo updateInfo;
    display(updateInfo);

    m_webPage->send(Messages::DrawingAreaProxy::Update(updateInfo));
    m_isWaitingForDidUpdate = true;
}

static bool shouldPaintBoundsRect(const IntRect& bounds, const Vector<IntRect>& rects)
{
    const size_t rectThreshold = 10;
    const float wastedSpaceThreshold = 0.75f;

    if (rects.size() <= 1 || rects.size() > rectThreshold)
        return true;

    // Attempt to guess whether or not we should use the region bounds rect or the individual rects.
    // We do this by computing the percentage of "wasted space" in the bounds.  If that wasted space
    // is too large, then we will do individual rect painting instead.
    unsigned boundsArea = bounds.width() * bounds.height();
    unsigned rectsArea = 0;
    for (size_t i = 0; i < rects.size(); ++i)
        rectsArea += rects[i].width() * rects[i].height();

    float wastedSpace = 1 - (rectsArea / boundsArea);

    return wastedSpace <= wastedSpaceThreshold;
}

void DrawingAreaImpl::display(UpdateInfo& updateInfo)
{
    ASSERT(!m_isPaintingSuspended);

    // FIXME: It would be better if we could avoid painting altogether when there is a custom representation.
    if (m_webPage->mainFrameHasCustomRepresentation())
        return;

    IntRect bounds = m_dirtyRegion.bounds();
    Vector<IntRect> rects = m_dirtyRegion.rects();

    if (shouldPaintBoundsRect(bounds, rects)) {
        rects.clear();
        rects.append(bounds);
    }

    updateInfo.scrollRect = m_scrollRect;
    updateInfo.scrollOffset = m_scrollOffset;

    m_dirtyRegion = Region();
    m_scrollRect = IntRect();
    m_scrollOffset = IntSize();
    
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(bounds.size());
    if (!bitmap->createHandle(updateInfo.bitmapHandle))
        return;

    OwnPtr<GraphicsContext> graphicsContext = bitmap->createGraphicsContext();

    m_webPage->layoutIfNeeded();
    
    updateInfo.timestamp = currentTime();
    updateInfo.viewSize = m_webPage->size();
    updateInfo.updateRectBounds = bounds;

    graphicsContext->translate(-bounds.x(), -bounds.y());

    for (size_t i = 0; i < rects.size(); ++i) {
        m_webPage->drawRect(*graphicsContext, rects[i]);
        updateInfo.updateRects.append(rects[i]);
    }
        
    // Layout can trigger more calls to setNeedsDisplay and we don't want to process them
    // until the UI process has painted the update, so we stop the timer here.
    m_displayTimer.stop();
}


} // namespace WebKit
