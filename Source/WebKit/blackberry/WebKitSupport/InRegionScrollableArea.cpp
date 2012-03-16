/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "InRegionScrollableArea.h"

#include "Frame.h"
#include "NotImplemented.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "WebPage_p.h"

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

InRegionScrollableArea::InRegionScrollableArea()
    : m_webPage(0)
    , m_layer(0)
{
}

InRegionScrollableArea::InRegionScrollableArea(WebPagePrivate* webPage, RenderLayer* layer)
    : m_webPage(webPage)
    , m_layer(layer)
{
    ASSERT(webPage);
    ASSERT(layer);
    m_isNull = false;

    // FIXME: Add an ASSERT here as the 'layer' must be scrollable.

    RenderObject* layerRenderer = layer->renderer();
    ASSERT(layerRenderer);

    if (layerRenderer->isRenderView()) { // #document case

        FrameView* view = toRenderView(layerRenderer)->frameView();
        ASSERT(view);

        Frame* frame = view->frame();
        ASSERT_UNUSED(frame, frame);

        m_scrollPosition = m_webPage->mapToTransformed(view->scrollPosition());
        m_contentsSize = m_webPage->mapToTransformed(view->contentsSize());
        m_viewportSize = m_webPage->mapToTransformed(view->visibleContentRect(false /*includeScrollbars*/)).size();

        m_visibleWindowRect = m_webPage->mapToTransformed(m_webPage->getRecursiveVisibleWindowRect(view));
        IntRect transformedWindowRect = IntRect(IntPoint::zero(), m_webPage->transformedViewportSize());
        m_visibleWindowRect.intersect(transformedWindowRect);

        m_scrollsHorizontally = view->contentsWidth() > view->visibleWidth();
        m_scrollsVertically = view->contentsHeight() > view->visibleHeight();

        m_minimumScrollPosition = m_webPage->mapToTransformed(calculateMinimumScrollPosition(
            view->visibleContentRect().size(),
            0.0 /*overscrollLimit*/));
        m_maximumScrollPosition = m_webPage->mapToTransformed(calculateMaximumScrollPosition(
            view->visibleContentRect().size(),
            view->contentsSize(),
            0.0 /*overscrollLimit*/));

    } else { // RenderBox-based elements case (scrollable boxes (div's, p's, textarea's, etc)).

        RenderBox* box = m_layer->renderBox();
        ASSERT(box);
        ASSERT(box->canBeScrolledAndHasScrollableArea());

        ScrollableArea* scrollableArea = static_cast<ScrollableArea*>(m_layer);
        m_scrollPosition = m_webPage->mapToTransformed(scrollableArea->scrollPosition());
        m_contentsSize = m_webPage->mapToTransformed(scrollableArea->contentsSize());
        m_viewportSize = m_webPage->mapToTransformed(scrollableArea->visibleContentRect(false /*includeScrollbars*/)).size();

        m_visibleWindowRect = m_layer->renderer()->absoluteClippedOverflowRect();
        m_visibleWindowRect = m_layer->renderer()->frame()->view()->contentsToWindow(m_visibleWindowRect);
        IntRect visibleFrameWindowRect = m_webPage->getRecursiveVisibleWindowRect(m_layer->renderer()->frame()->view());
        m_visibleWindowRect.intersect(visibleFrameWindowRect);
        m_visibleWindowRect = m_webPage->mapToTransformed(m_visibleWindowRect);
        IntRect transformedWindowRect = IntRect(IntPoint::zero(), m_webPage->transformedViewportSize());
        m_visibleWindowRect.intersect(transformedWindowRect);

        m_scrollsHorizontally = box->scrollWidth() != box->clientWidth() && box->scrollsOverflowX();
        m_scrollsVertically = box->scrollHeight() != box->clientHeight() && box->scrollsOverflowY();

        m_minimumScrollPosition = m_webPage->mapToTransformed(calculateMinimumScrollPosition(
            Platform::IntSize(box->clientWidth(), box->clientHeight()),
            0.0 /*overscrollLimit*/));
        m_maximumScrollPosition = m_webPage->mapToTransformed(calculateMaximumScrollPosition(
            Platform::IntSize(box->clientWidth(), box->clientHeight()),
            Platform::IntSize(box->scrollWidth(), box->scrollHeight()),
            0.0 /*overscrollLimit*/));
    }
}

Platform::IntPoint InRegionScrollableArea::calculateMinimumScrollPosition(const Platform::IntSize& viewportSize, float overscrollLimitFactor) const
{
    // FIXME: Eventually we should support overscroll like iOS5 does.
    ASSERT(!allowsOverscroll());

    return Platform::IntPoint(-(viewportSize.width() * overscrollLimitFactor),
                              -(viewportSize.height() * overscrollLimitFactor));
}

Platform::IntPoint InRegionScrollableArea::calculateMaximumScrollPosition(const Platform::IntSize& viewportSize, const Platform::IntSize& contentsSize, float overscrollLimitFactor) const
{
    // FIXME: Eventually we should support overscroll like iOS5 does.
    ASSERT(!allowsOverscroll());

    return Platform::IntPoint(std::max(contentsSize.width() - viewportSize.width(), 0) + overscrollLimitFactor,
                              std::max(contentsSize.height() - viewportSize.height(), 0) + overscrollLimitFactor);
}

RenderLayer* InRegionScrollableArea::layer() const
{
    ASSERT(!m_isNull);
    return m_layer;
}
}

}
