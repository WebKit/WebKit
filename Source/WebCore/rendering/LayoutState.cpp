/*
 * Copyright (C) 2007, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "LayoutState.h"

#include "ColumnInfo.h"
#include "RenderFlowThread.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderView.h"

namespace WebCore {

LayoutState::LayoutState(std::unique_ptr<LayoutState> next, RenderBox* renderer, const LayoutSize& offset, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, ColumnInfo* columnInfo)
    : m_columnInfo(columnInfo)
    , m_lineGrid(0)
    , m_next(std::move(next))
#ifndef NDEBUG
    , m_renderer(renderer)
#endif
{
    ASSERT(m_next);

    bool fixed = renderer->isOutOfFlowPositioned() && renderer->style().position() == FixedPosition;
    if (fixed) {
        // FIXME: This doesn't work correctly with transforms.
        FloatPoint fixedOffset = renderer->view().localToAbsolute(FloatPoint(), IsFixed);
        m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y()) + offset;
    } else
        m_paintOffset = m_next->m_paintOffset + offset;

    if (renderer->isOutOfFlowPositioned() && !fixed) {
        if (RenderElement* container = renderer->container()) {
            if (container->isInFlowPositioned() && container->isRenderInline())
                m_paintOffset += toRenderInline(container)->offsetForInFlowPositionedInline(renderer);
        }
    }

    m_layoutOffset = m_paintOffset;

    if (renderer->isInFlowPositioned() && renderer->hasLayer())
        m_paintOffset += renderer->layer()->offsetForInFlowPosition();

    m_clipped = !fixed && m_next->m_clipped;
    if (m_clipped)
        m_clipRect = m_next->m_clipRect;

    if (renderer->hasOverflowClip()) {
        LayoutRect clipRect(toLayoutPoint(m_paintOffset) + renderer->view().layoutDelta(), renderer->cachedSizeForOverflowClip());
        if (m_clipped)
            m_clipRect.intersect(clipRect);
        else {
            m_clipRect = clipRect;
            m_clipped = true;
        }

        m_paintOffset -= renderer->scrolledContentOffset();
    }

    // If we establish a new page height, then cache the offset to the top of the first page.
    // We can compare this later on to figure out what part of the page we're actually on,
    if (pageLogicalHeight || m_columnInfo || renderer->isRenderFlowThread()) {
        m_pageLogicalHeight = pageLogicalHeight;
        bool isFlipped = renderer->style().isFlippedBlocksWritingMode();
        m_pageOffset = LayoutSize(m_layoutOffset.width() + (!isFlipped ? renderer->borderLeft() + renderer->paddingLeft() : renderer->borderRight() + renderer->paddingRight()),
                               m_layoutOffset.height() + (!isFlipped ? renderer->borderTop() + renderer->paddingTop() : renderer->borderBottom() + renderer->paddingBottom()));
        m_pageLogicalHeightChanged = pageLogicalHeightChanged;
    } else {
        // If we don't establish a new page height, then propagate the old page height and offset down.
        m_pageLogicalHeight = m_next->m_pageLogicalHeight;
        m_pageLogicalHeightChanged = m_next->m_pageLogicalHeightChanged;
        m_pageOffset = m_next->m_pageOffset;
        
        // Disable pagination for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and
        // writing mode roots.
        if (renderer->isUnsplittableForPagination())
            m_pageLogicalHeight = 0;
    }
    
    // Propagate line grid information.
    propagateLineGridInfo(renderer);

    if (!m_columnInfo)
        m_columnInfo = m_next->m_columnInfo;

    m_layoutDelta = m_next->m_layoutDelta;
#if !ASSERT_DISABLED && ENABLE(SATURATED_LAYOUT_ARITHMETIC)
    m_layoutDeltaXSaturated = m_next->m_layoutDeltaXSaturated;
    m_layoutDeltaYSaturated = m_next->m_layoutDeltaYSaturated;
#endif
    
    m_isPaginated = m_pageLogicalHeight || m_columnInfo || renderer->isRenderFlowThread();

    if (lineGrid() && (lineGrid()->style().writingMode() == renderer->style().writingMode()) && renderer->isRenderBlock())
        toRenderBlock(renderer)->computeLineGridPaginationOrigin(*this);

    // If we have a new grid to track, then add it to our set.
    if (renderer->style().lineGrid() != RenderStyle::initialLineGrid() && renderer->isRenderBlockFlow())
        establishLineGrid(toRenderBlockFlow(renderer));

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

LayoutState::LayoutState(RenderObject& root)
    : m_clipped(false)
    , m_isPaginated(false)
    , m_pageLogicalHeightChanged(false)
#if !ASSERT_DISABLED && ENABLE(SATURATED_LAYOUT_ARITHMETIC)
    , m_layoutDeltaXSaturated(false)
    , m_layoutDeltaYSaturated(false)
#endif    
    , m_columnInfo(0)
    , m_lineGrid(0)
    , m_pageLogicalHeight(0)
#ifndef NDEBUG
    , m_renderer(&root)
#endif
{
    RenderElement* container = root.container();
    FloatPoint absContentPoint = container->localToAbsolute(FloatPoint(), UseTransforms);
    m_paintOffset = LayoutSize(absContentPoint.x(), absContentPoint.y());

    if (container->hasOverflowClip()) {
        m_clipped = true;
        RenderBox* containerBox = toRenderBox(container);
        m_clipRect = LayoutRect(toLayoutPoint(m_paintOffset), containerBox->cachedSizeForOverflowClip());
        m_paintOffset -= containerBox->scrolledContentOffset();
    }
}
void LayoutState::clearPaginationInformation()
{
    m_pageLogicalHeight = m_next->m_pageLogicalHeight;
    m_pageOffset = m_next->m_pageOffset;
    m_columnInfo = m_next->m_columnInfo;
}

LayoutUnit LayoutState::pageLogicalOffset(RenderBox* child, LayoutUnit childLogicalOffset) const
{
    if (child->isHorizontalWritingMode())
        return m_layoutOffset.height() + childLogicalOffset - m_pageOffset.height();
    return m_layoutOffset.width() + childLogicalOffset - m_pageOffset.width();
}

void LayoutState::addForcedColumnBreak(RenderBox* child, LayoutUnit childLogicalOffset)
{
    if (!m_columnInfo || m_columnInfo->columnHeight())
        return;
    m_columnInfo->addForcedBreak(pageLogicalOffset(child, childLogicalOffset));
}

void LayoutState::propagateLineGridInfo(RenderBox* renderer)
{
    // Disable line grids for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and
    // writing mode roots.
    if (!m_next || renderer->isUnsplittableForPagination())
        return;

    m_lineGrid = m_next->m_lineGrid;
    m_lineGridOffset = m_next->m_lineGridOffset;
    m_lineGridPaginationOrigin = m_next->m_lineGridPaginationOrigin;
}

void LayoutState::establishLineGrid(RenderBlockFlow* block)
{
    // First check to see if this grid has been established already.
    if (m_lineGrid) {
        if (m_lineGrid->style().lineGrid() == block->style().lineGrid())
            return;
        RenderBlockFlow* currentGrid = m_lineGrid;
        for (LayoutState* currentState = m_next.get(); currentState; currentState = currentState->m_next.get()) {
            if (currentState->m_lineGrid == currentGrid)
                continue;
            currentGrid = currentState->m_lineGrid;
            if (!currentGrid)
                break;
            if (currentGrid->style().lineGrid() == block->style().lineGrid()) {
                m_lineGrid = currentGrid;
                m_lineGridOffset = currentState->m_lineGridOffset;
                return;
            }
        }
    }
    
    // We didn't find an already-established grid with this identifier. Our render object establishes the grid.
    m_lineGrid = block;
    m_lineGridOffset = m_layoutOffset; 
}

} // namespace WebCore
