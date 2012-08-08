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
#include "EflViewportHandler.h"

#if USE(COORDINATED_GRAPHICS)

#include "LayerTreeCoordinatorProxy.h"
#include "LayerTreeRenderer.h"
#include "TransformationMatrix.h"

using namespace WebCore;

namespace WebKit {

EflViewportHandler::EflViewportHandler(PageClientImpl* pageClientImpl)
    : m_pageClientImpl(pageClientImpl)
    , m_scaleFactor(1)
{
    ASSERT(m_pageClientImpl);
}

EflViewportHandler::~EflViewportHandler()
{
}

void EflViewportHandler::display(const IntRect& rect)
{
    WebCore::TransformationMatrix matrix;
    matrix.setMatrix(m_scaleFactor, 0, 0, m_scaleFactor, -m_visibleContentRect.x(), -m_visibleContentRect.y());

    LayerTreeRenderer* renderer = drawingArea()->layerTreeCoordinatorProxy()->layerTreeRenderer();
    renderer->setActive(true);
    renderer->syncRemoteContent();
    renderer->paintToCurrentGLContext(matrix, 1, rect);
}

void EflViewportHandler::updateViewportSize(const IntSize& viewportSize)
{
    m_viewportSize = viewportSize;
    m_pageClientImpl->page()->setViewportSize(viewportSize);
    setVisibleContentsRect(m_visibleContentRect.location(), m_scaleFactor, FloatPoint());
}

void EflViewportHandler::setVisibleContentsRect(const IntPoint& newScrollPosition, float newScale, const FloatPoint& trajectory)
{
    m_scaleFactor = newScale;
    m_visibleContentRect = IntRect(newScrollPosition, m_viewportSize);

    // Move visibleContentRect inside contentsRect when visibleContentRect goes outside contentsRect. 
    IntSize contentsSize = m_contentsSize;
    contentsSize.scale(m_scaleFactor);
    if (m_visibleContentRect.x() > contentsSize.width() - m_visibleContentRect.width())
        m_visibleContentRect.setX(contentsSize.width() - m_visibleContentRect.width());
    if (m_visibleContentRect.x() < 0)
        m_visibleContentRect.setX(0);
    if (m_visibleContentRect.y() > contentsSize.height() - m_visibleContentRect.height())
        m_visibleContentRect.setY(contentsSize.height() - m_visibleContentRect.height());
    if (m_visibleContentRect.y() < 0)
        m_visibleContentRect.setY(0);

    FloatRect mapRectToWebContent = m_visibleContentRect;
    mapRectToWebContent.scale(1 / m_scaleFactor);
    drawingArea()->setVisibleContentsRect(enclosingIntRect(mapRectToWebContent), m_scaleFactor, trajectory);
}

void EflViewportHandler::didChangeContentsSize(const WebCore::IntSize& size)
{
    m_contentsSize = size;
    setVisibleContentsRect(m_visibleContentRect.location(), m_scaleFactor, FloatPoint());
    drawingArea()->layerTreeCoordinatorProxy()->setContentsSize(WebCore::FloatSize(size.width(), size.height()));
}

} // namespace WebKit
#endif // USE(COORDINATED_GRAPHICS)

