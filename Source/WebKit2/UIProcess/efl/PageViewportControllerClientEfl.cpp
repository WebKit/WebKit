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

#include "EwkViewImpl.h"
#include "LayerTreeCoordinatorProxy.h"
#include "LayerTreeRenderer.h"
#include "PageViewportController.h"
#include "TransformationMatrix.h"
#include "ewk_view_private.h"

using namespace WebCore;

namespace WebKit {

PageViewportControllerClientEfl::PageViewportControllerClientEfl(EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
    , m_scaleFactor(1)
    , m_controller(0)
{
    ASSERT(m_viewImpl);
}

PageViewportControllerClientEfl::~PageViewportControllerClientEfl()
{
}

DrawingAreaProxy* PageViewportControllerClientEfl::drawingArea() const
{
    return m_viewImpl->page()->drawingArea();
}

void PageViewportControllerClientEfl::setRendererActive(bool active)
{
    drawingArea()->layerTreeCoordinatorProxy()->layerTreeRenderer()->setActive(active);
}

void PageViewportControllerClientEfl::updateViewportSize(const IntSize& viewportSize)
{
    m_viewportSize = viewportSize;

    ASSERT(m_controller);
    m_controller->didChangeViewportSize(viewportSize);
}

void PageViewportControllerClientEfl::setVisibleContentsRect(const IntPoint& newScrollPosition, float newScale, const FloatPoint& trajectory)
{
    m_scaleFactor = newScale;
    m_scrollPosition = newScrollPosition;

    ASSERT(m_controller);
    m_controller->didChangeContentsVisibility(m_scrollPosition, m_scaleFactor, trajectory);
}

void PageViewportControllerClientEfl::didChangeContentsSize(const WebCore::IntSize&)
{
    m_viewImpl->update();
}

void PageViewportControllerClientEfl::setViewportPosition(const WebCore::FloatPoint& contentsPoint)
{
    IntPoint position(contentsPoint.x(), contentsPoint.y());
    setVisibleContentsRect(position, m_scaleFactor, FloatPoint());
    m_viewImpl->setScrollPosition(position);
}

void PageViewportControllerClientEfl::setContentsScale(float newScale, bool treatAsInitialValue)
{
    if (treatAsInitialValue) {
        m_scrollPosition = IntPoint();
        m_viewImpl->setScrollPosition(IntPoint());
    }
    m_scaleFactor = newScale;
    m_viewImpl->setScaleFactor(newScale);
}

void PageViewportControllerClientEfl::didResumeContent()
{
    ASSERT(m_controller);
    m_controller->didChangeContentsVisibility(m_scrollPosition, m_scaleFactor);
}

void PageViewportControllerClientEfl::didChangeVisibleContents()
{
    m_viewImpl->update();
}

void PageViewportControllerClientEfl::didChangeViewportAttributes()
{
}

void PageViewportControllerClientEfl::setController(PageViewportController* controller)
{
    m_controller = controller;
}

} // namespace WebKit
#endif // USE(TILED_BACKING_STORE)

