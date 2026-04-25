/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "ScrollbarUpdateScope.h"

#include "InspectorInstrumentation.h"
#include "Logging.h"
#include "RenderBoxInlines.h"
#include "RenderLayerScrollableArea.h"
#include "ScrollAnchoringController.h"


namespace WebCore {

ScrollbarUpdateScope::ScrollbarUpdateScope(RenderLayerScrollableArea& scrollableArea, ScrollPosition originalScrollPosition, OptionSet<ScrollbarChange> scrollbarChanges, HasHorizontalOverflow hasHorizontalOverflow, HasVerticalOverflow hasVerticalOverflow)
    : m_renderLayerScrollableArea(scrollableArea)
    , m_originalScrollPosition(originalScrollPosition)
    , m_scrollbarChanges(scrollbarChanges)
    , m_hasHorizontalOverflow(hasHorizontalOverflow)
    , m_hasVerticalOverflow(hasVerticalOverflow)
{
}

ScrollbarUpdateScope::~ScrollbarUpdateScope()
{
    CheckedPtr box = m_renderLayerScrollableArea->m_layer.renderBox();
    ASSERT(box);

    // Set up the range.
    if (RefPtr hBar = m_renderLayerScrollableArea->m_hBar)
        hBar->setProportion(roundToInt(box->clientWidth()), m_renderLayerScrollableArea->m_scrollWidth);
    if (RefPtr vBar = m_renderLayerScrollableArea->m_vBar)
        vBar->setProportion(roundToInt(box->clientHeight()), m_renderLayerScrollableArea->m_scrollHeight);

    m_renderLayerScrollableArea->updateScrollbarSteps();

    auto hasScrollableOverflow = [&]() {
        if (m_hasVerticalOverflow == HasVerticalOverflow::Yes && m_renderLayerScrollableArea->m_layer.renderBox()->scrollsOverflowY())
            return true;

        if (m_hasHorizontalOverflow == HasHorizontalOverflow::Yes && m_renderLayerScrollableArea->m_layer.renderBox()->scrollsOverflowX())
            return true;

        return false;
    };

    m_renderLayerScrollableArea->updateScrollableAreaSet(hasScrollableOverflow());

    if (CheckedPtr scrollAnchoringController = m_renderLayerScrollableArea->scrollAnchoringController())
        scrollAnchoringController->scrollerDidLayout();

    LOG_WITH_STREAM(Scrolling, stream << "RenderLayerScrollableArea [" << m_renderLayerScrollableArea->scrollingNodeID() << "] updateScrollInfoAfterLayout - new scroll width " << m_renderLayerScrollableArea->m_scrollWidth << " scroll height " << m_renderLayerScrollableArea->m_scrollHeight
        << " rubber banding " << m_renderLayerScrollableArea->isRubberBandInProgress() << " user scrolling " << m_renderLayerScrollableArea->isUserScrollInProgress() << " scroll position updated from " << m_originalScrollPosition << " to " << m_renderLayerScrollableArea->scrollPosition());

    if (m_originalScrollPosition != m_renderLayerScrollableArea->scrollPosition())
        m_renderLayerScrollableArea->scrollToPositionWithoutAnimation(IntPoint(m_renderLayerScrollableArea->scrollPosition()));

    if (m_renderLayerScrollableArea->m_layer.isComposited()) {
        m_renderLayerScrollableArea->m_layer.setNeedsCompositingGeometryUpdate();
        m_renderLayerScrollableArea->m_layer.setNeedsCompositingConfigurationUpdate();
    }

    if (m_renderLayerScrollableArea->canUseCompositedScrolling())
        m_renderLayerScrollableArea->m_layer.setNeedsPostLayoutCompositingUpdate();

    m_renderLayerScrollableArea->resnapAfterLayout();

    InspectorInstrumentation::didAddOrRemoveScrollbars(m_renderLayerScrollableArea->m_layer.renderer());
}

} // namespace WebCore
