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

#include "Document.h"
#include "Frame.h"
#include "LayerWebKitThread.h"
#include "InRegionScroller_p.h"
#include "RenderBox.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLayerCompositor.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "WebPage_p.h"

using namespace WebCore;

namespace BlackBerry {
namespace WebKit {

InRegionScrollableArea::InRegionScrollableArea()
    : m_webPage(0)
    , m_layer(0)
    , m_hasWindowVisibleRectCalculated(false)
{
}

InRegionScrollableArea::~InRegionScrollableArea()
{
    if (m_cachedCompositedScrollableLayer)
        m_cachedCompositedScrollableLayer->clearOverride();
}

InRegionScrollableArea::InRegionScrollableArea(WebPagePrivate* webPage, RenderLayer* layer)
    : m_webPage(webPage)
    , m_layer(layer)
    , m_document(0)
    , m_hasWindowVisibleRectCalculated(false)
{
    ASSERT(webPage);
    ASSERT(layer);
    m_isNull = false;

    // Add a pointer to the enclosing document as the pointer to layer or node along the way may become invalid.
    if (m_layer->enclosingElement())
        m_document = m_layer->enclosingElement()->document();

    // FIXME: Add an ASSERT here as the 'layer' must be scrollable.

    RenderObject* layerRenderer = layer->renderer();
    ASSERT(layerRenderer);

    if (layerRenderer->isRenderView()) { // #document case

        RenderView* renderView = toRenderView(layerRenderer);
        ASSERT(renderView);

        FrameView* view = toRenderView(layerRenderer)->frameView();
        ASSERT(view);

        Frame* frame = view->frame();
        ASSERT_UNUSED(frame, frame);

        m_scrollPosition = m_webPage->mapToTransformed(view->scrollPosition());
        m_contentsSize = m_webPage->mapToTransformed(view->contentsSize());
        m_viewportSize = m_webPage->mapToTransformed(view->visibleContentRect(false /*includeScrollbars*/)).size();

        m_scrollsHorizontally = view->contentsWidth() > view->visibleWidth();
        m_scrollsVertically = view->contentsHeight() > view->visibleHeight();

        m_supportsCompositedScrolling = true;

        m_scrollTarget = InnerFrame;

        ASSERT(!m_cachedNonCompositedScrollableNode);

        m_camouflagedCompositedScrollableLayer = reinterpret_cast<unsigned>(renderView->compositor()->scrollLayer()->platformLayer());
        m_cachedCompositedScrollableLayer = renderView->compositor()->scrollLayer()->platformLayer();

    } else { // RenderBox-based elements case (scrollable boxes (div's, p's, textarea's, etc)).

        RenderBox* box = m_layer->renderBox();
        ASSERT(box);
        ASSERT(InRegionScrollerPrivate::canScrollRenderBox(box));

        ScrollableArea* scrollableArea = static_cast<ScrollableArea*>(m_layer);
        m_scrollPosition = m_webPage->mapToTransformed(scrollableArea->scrollPosition());
        m_contentsSize = m_webPage->mapToTransformed(scrollableArea->contentsSize());
        m_viewportSize = m_webPage->mapToTransformed(scrollableArea->visibleContentRect(false /*includeScrollbars*/)).size();

        m_scrollsHorizontally = box->scrollWidth() != box->clientWidth() && box->scrollsOverflowX();
        m_scrollsVertically = box->scrollHeight() != box->clientHeight() && box->scrollsOverflowY();

        m_scrollTarget = BlockElement;

        // Both caches below are self-exclusive.
        if (m_layer->usesCompositedScrolling()) {
            m_forceContentToBeVerticallyScrollable = true;
            m_supportsCompositedScrolling = true;
            ASSERT(m_layer->backing()->hasScrollingLayer());
            m_camouflagedCompositedScrollableLayer = reinterpret_cast<unsigned>(m_layer->backing()->scrollingContentsLayer()->platformLayer());
            m_cachedCompositedScrollableLayer = m_layer->backing()->scrollingContentsLayer()->platformLayer();
            ASSERT(!m_cachedNonCompositedScrollableNode);
        } else {
            m_camouflagedCompositedScrollableLayer = reinterpret_cast<unsigned>(m_layer->enclosingElement());
            m_cachedNonCompositedScrollableNode = m_layer->enclosingElement();
            ASSERT(!m_cachedCompositedScrollableLayer);
        }
    }
}

void InRegionScrollableArea::setVisibleWindowRect(const WebCore::IntRect& rect)
{
    m_hasWindowVisibleRectCalculated = true;
    m_visibleWindowRect = rect;
}

Platform::IntRect InRegionScrollableArea::visibleWindowRect() const
{
    ASSERT(m_hasWindowVisibleRectCalculated);
    return m_visibleWindowRect;
}

RenderLayer* InRegionScrollableArea::layer() const
{
    ASSERT(!m_isNull);
    return m_layer;
}

Document* InRegionScrollableArea::document() const
{
    ASSERT(!m_isNull);
    return m_document;
}

LayerWebKitThread* InRegionScrollableArea::cachedScrollableLayer() const
{
    return m_cachedCompositedScrollableLayer.get();
}

Node* InRegionScrollableArea::cachedScrollableNode() const
{
    return m_cachedNonCompositedScrollableNode.get();
}

}
}
