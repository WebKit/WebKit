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
#include "WebProcess.h"
#include <WebCore/GraphicsContext.h>

#ifndef __APPLE__
#error "This drawing area is not ready for use by other ports yet."
#endif

using namespace WebCore;

namespace WebKit {

PassRefPtr<DrawingAreaImpl> DrawingAreaImpl::create(DrawingAreaInfo::Identifier identifier, WebPage* webPage)
{
    return adoptRef(new DrawingAreaImpl(identifier, webPage));
}

DrawingAreaImpl::~DrawingAreaImpl()
{
}

DrawingAreaImpl::DrawingAreaImpl(DrawingAreaInfo::Identifier identifier, WebPage* webPage)
    : DrawingArea(DrawingAreaInfo::Impl, identifier, webPage)
    , m_isWaitingForDidUpdate(false)
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

void DrawingAreaImpl::scroll(const IntSize& scrollDelta, const IntRect& rectToScroll, const IntRect& clipRect)
{
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

    // FIXME: Repaint.

    m_webPage->send(Messages::DrawingAreaProxy::DidSetSize());
}

void DrawingAreaImpl::didUpdate()
{
    m_isWaitingForDidUpdate = false;

    // Display if needed.
    display();
}

void DrawingAreaImpl::scheduleDisplay()
{
    if (m_isWaitingForDidUpdate)
        return;

    if (m_dirtyRegion.isEmpty())
        return;

    if (m_displayTimer.isActive())
        return;

    m_displayTimer.startOneShot(0);
}

void DrawingAreaImpl::display()
{
    if (m_dirtyRegion.isEmpty())
        return;

    UpdateInfo updateInfo;
    display(updateInfo);

    m_webPage->send(Messages::DrawingAreaProxy::Update(updateInfo));
    m_isWaitingForDidUpdate = true;
}

void DrawingAreaImpl::display(UpdateInfo& updateInfo)
{
    // FIXME: It would be better if we could avoid painting altogether when there is a custom representation.
    if (m_webPage->mainFrameHasCustomRepresentation())
        return;

    IntRect bounds = m_dirtyRegion.bounds();
    Vector<IntRect> rects = m_dirtyRegion.rects();

    m_dirtyRegion = Region();

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(bounds.size());

    OwnPtr<GraphicsContext> graphicsContext = bitmap->createGraphicsContext();

    m_webPage->layoutIfNeeded();
    
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
