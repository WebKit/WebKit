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

#include "CoordinatedLayerTreeHostProxy.h"
#include "EwkViewImpl.h"
#include "LayerTreeRenderer.h"
#include "PageViewportController.h"
#include "TransformationMatrix.h"

using namespace WebCore;

namespace WebKit {

PageViewportControllerClientEfl::PageViewportControllerClientEfl(EwkViewImpl* viewImpl)
    : m_viewImpl(viewImpl)
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
    drawingArea()->coordinatedLayerTreeHostProxy()->layerTreeRenderer()->setActive(active);
}

void PageViewportControllerClientEfl::updateViewportSize()
{
    ASSERT(m_controller);
    m_controller->didChangeViewportSize(m_viewImpl->size());
}

void PageViewportControllerClientEfl::didChangeContentsSize(const WebCore::IntSize& contentsSize)
{
    drawingArea()->coordinatedLayerTreeHostProxy()->setContentsSize(contentsSize);
    m_viewImpl->update();
}

void PageViewportControllerClientEfl::setViewportPosition(const WebCore::FloatPoint& contentsPoint)
{
    m_contentPosition = contentsPoint;

    FloatPoint pos(contentsPoint);
    pos.scale(scaleFactor(), scaleFactor());
    m_viewImpl->setPagePosition(pos);

    m_controller->didChangeContentsVisibility(m_contentPosition, scaleFactor());
}

void PageViewportControllerClientEfl::setContentsScale(float newScale)
{
    m_viewImpl->setScaleFactor(newScale);
}

void PageViewportControllerClientEfl::didResumeContent()
{
    ASSERT(m_controller);
    m_controller->didChangeContentsVisibility(m_contentPosition, scaleFactor());
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

