/*
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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
#include "PageViewportControllerClientEfl.h"

#if USE(TILED_BACKING_STORE)

#include "LayerTreeCoordinatorProxy.h"
#include "LayerTreeRenderer.h"
#include "PageViewportController.h"
#include "TransformationMatrix.h"
#include "ewk_view_private.h"

using namespace WebCore;

namespace WebKit {

PageViewportControllerClientEfl::PageViewportControllerClientEfl(Evas_Object* viewWidget)
    : m_viewWidget(viewWidget)
    , m_scaleFactor(1)
    , m_pageViewportController(0)
{
    ASSERT(m_viewWidget);
}

PageViewportControllerClientEfl::~PageViewportControllerClientEfl()
{
}

DrawingAreaProxy* PageViewportControllerClientEfl::drawingArea() const
{
    return ewk_view_page_get(m_viewWidget)->drawingArea();
}

void PageViewportControllerClientEfl::setRendererActive(bool active)
{
    drawingArea()->layerTreeCoordinatorProxy()->layerTreeRenderer()->setActive(active);
}

void PageViewportControllerClientEfl::display(const IntRect& rect, const IntPoint& viewPosition)
{
    WebCore::TransformationMatrix matrix;
    matrix.setMatrix(m_scaleFactor, 0, 0, m_scaleFactor, -m_scrollPosition.x() * m_scaleFactor + viewPosition.x() , -m_scrollPosition.y() * m_scaleFactor + viewPosition.y());

    LayerTreeRenderer* renderer = drawingArea()->layerTreeCoordinatorProxy()->layerTreeRenderer();
    renderer->setActive(true);
    renderer->syncRemoteContent();
    IntRect clipRect(rect);
    clipRect.move(viewPosition.x(), viewPosition.y());
    renderer->paintToCurrentGLContext(matrix, 1, clipRect);
}

void PageViewportControllerClientEfl::updateViewportSize(const IntSize& viewportSize)
{
    m_viewportSize = viewportSize;
    ewk_view_page_get(m_viewWidget)->setViewportSize(viewportSize);
    m_pageViewportController->didChangeViewportSize(viewportSize);
}

void PageViewportControllerClientEfl::setVisibleContentsRect(const IntPoint& newScrollPosition, float newScale, const FloatPoint& /*trajectory*/)
{
    m_scaleFactor = newScale;
    m_scrollPosition = newScrollPosition;
    m_pageViewportController->didChangeContentsVisibility(m_scrollPosition, m_scaleFactor, FloatPoint());
}

void PageViewportControllerClientEfl::didChangeContentsSize(const WebCore::IntSize& size)
{
    m_contentsSize = size;
    IntRect rect = IntRect(IntPoint(), m_viewportSize);
    ewk_view_display(m_viewWidget, rect);
}

void PageViewportControllerClientEfl::setViewportPosition(const WebCore::FloatPoint& contentsPoint)
{
    setVisibleContentsRect(IntPoint(contentsPoint.x(), contentsPoint.y()), m_scaleFactor, FloatPoint());
}

void PageViewportControllerClientEfl::setContentsScale(float newScale, bool treatAsInitialValue)
{
    if (treatAsInitialValue)
        m_scrollPosition = IntPoint();
    m_scaleFactor = newScale;
}

void PageViewportControllerClientEfl::didResumeContent()
{
    m_pageViewportController->didChangeContentsVisibility(m_scrollPosition, m_scaleFactor);
}

void PageViewportControllerClientEfl::didChangeVisibleContents()
{
    IntRect rect = IntRect(IntPoint(), m_viewportSize);
    ewk_view_display(m_viewWidget, rect);
}

void PageViewportControllerClientEfl::didChangeViewportAttributes()
{
}

void PageViewportControllerClientEfl::setController(PageViewportController* pageViewportController)
{
    m_pageViewportController = pageViewportController;
}

} // namespace WebKit
#endif // USE(TILED_BACKING_STORE)

