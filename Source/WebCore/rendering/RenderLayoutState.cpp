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
#include "RenderLayoutState.h"

#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderMultiColumnFlow.h"
#include "RenderView.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

RenderLayoutState::RenderLayoutState(RenderElement& renderer, IsPaginated isPaginated)
    : m_clipped(false)
    , m_isPaginated(isPaginated == IsPaginated::Yes)
    , m_pageLogicalHeightChanged(false)
#if ASSERT_ENABLED
    , m_layoutDeltaXSaturated(false)
    , m_layoutDeltaYSaturated(false)
    , m_renderer(&renderer)
#endif
{
    if (RenderElement* container = renderer.container()) {
        FloatPoint absContentPoint = container->localToAbsolute(FloatPoint(), UseTransforms);
        m_paintOffset = LayoutSize(absContentPoint.x(), absContentPoint.y());

        if (container->hasNonVisibleOverflow()) {
            m_clipped = true;
            auto& containerBox = downcast<RenderBox>(*container);
            m_clipRect = LayoutRect(toLayoutPoint(m_paintOffset), containerBox.cachedSizeForOverflowClip());
            m_paintOffset -= toLayoutSize(containerBox.scrollPosition());
        }
    }
    if (m_isPaginated) {
        // This is just a flag for known page height (see RenderBlockFlow::checkForPaginationLogicalHeightChange).
        m_pageLogicalHeight = 1;
    }
}

RenderLayoutState::RenderLayoutState(const FrameViewLayoutContext::LayoutStateStack& layoutStateStack, RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, std::optional<size_t> maximumLineCountForLineClamp, std::optional<size_t> visibleLineCountForLineClamp)
    : m_clipped(false)
    , m_isPaginated(false)
    , m_pageLogicalHeightChanged(false)
#if ASSERT_ENABLED
    , m_layoutDeltaXSaturated(false)
    , m_layoutDeltaYSaturated(false)
#endif
    , m_maximumLineCountForLineClamp(maximumLineCountForLineClamp)
    , m_visibleLineCountForLineClamp(visibleLineCountForLineClamp)
#if ASSERT_ENABLED
    , m_renderer(&renderer)
#endif
{
    if (!layoutStateStack.isEmpty()) {
        auto& ancestor = *layoutStateStack.last().get();
        computeOffsets(ancestor, renderer, offset);
        computeClipRect(ancestor, renderer);
    }
    computePaginationInformation(layoutStateStack, renderer, pageLogicalHeight, pageLogicalHeightChanged);
}

void RenderLayoutState::computeOffsets(const RenderLayoutState& ancestor, RenderBox& renderer, LayoutSize offset)
{
    bool fixed = renderer.isFixedPositioned();
    if (fixed) {
        FloatPoint fixedOffset = renderer.view().localToAbsolute(FloatPoint(), IsFixed);
        m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y()) + offset;
    } else
        m_paintOffset = ancestor.paintOffset() + offset;

    if (renderer.isOutOfFlowPositioned() && !fixed) {
        if (auto* container = renderer.container()) {
            if (container->isInFlowPositioned() && is<RenderInline>(*container))
                m_paintOffset += downcast<RenderInline>(*container).offsetForInFlowPositionedInline(&renderer);
        }
    }

    m_layoutOffset = m_paintOffset;

    if (renderer.isInFlowPositioned() && renderer.hasLayer())
        m_paintOffset += renderer.layer()->offsetForInFlowPosition();

    if (renderer.hasNonVisibleOverflow())
        m_paintOffset -= toLayoutSize(renderer.scrollPosition());

    m_layoutDelta = ancestor.layoutDelta();
#if ASSERT_ENABLED
    m_layoutDeltaXSaturated = ancestor.m_layoutDeltaXSaturated;
    m_layoutDeltaYSaturated = ancestor.m_layoutDeltaYSaturated;
#endif
}

void RenderLayoutState::computeClipRect(const RenderLayoutState& ancestor, RenderBox& renderer)
{
    m_clipped = !renderer.isFixedPositioned() && ancestor.isClipped();
    if (m_clipped)
        m_clipRect = ancestor.clipRect();
    if (!renderer.hasNonVisibleOverflow())
        return;

    auto paintOffsetForClipRect = toLayoutPoint(m_paintOffset + toLayoutSize(renderer.scrollPosition()));
    LayoutRect clipRect(paintOffsetForClipRect + renderer.view().frameView().layoutContext().layoutDelta(), renderer.cachedSizeForOverflowClip());
    if (m_clipped)
        m_clipRect.intersect(clipRect);
    else
        m_clipRect = clipRect;
    m_clipped = true;
    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

void RenderLayoutState::computePaginationInformation(const FrameViewLayoutContext::LayoutStateStack& layoutStateStack, RenderBox& renderer, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged)
{
    auto* ancestor = layoutStateStack.isEmpty() ? nullptr : layoutStateStack.last().get();
    // If we establish a new page height, then cache the offset to the top of the first page.
    // We can compare this later on to figure out what part of the page we're actually on.
    if (pageLogicalHeight || renderer.isRenderFragmentedFlow()) {
        m_pageLogicalHeight = pageLogicalHeight;
        bool isFlipped = renderer.style().isFlippedBlocksWritingMode();
        m_pageOffset = LayoutSize(m_layoutOffset.width() + (!isFlipped ? renderer.borderLeft() + renderer.paddingLeft() : renderer.borderRight() + renderer.paddingRight()), m_layoutOffset.height() + (!isFlipped ? renderer.borderTop() + renderer.paddingTop() : renderer.borderBottom() + renderer.paddingBottom()));
        m_pageLogicalHeightChanged = pageLogicalHeightChanged;
        m_isPaginated = true;
    } else if (ancestor) {
        // If we don't establish a new page height, then propagate the old page height and offset down.
        m_pageLogicalHeight = ancestor->pageLogicalHeight();
        m_pageLogicalHeightChanged = ancestor->pageLogicalHeightChanged();
        m_pageOffset = ancestor->pageOffset();

        // Disable pagination for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and writing mode roots.
        if (renderer.isUnsplittableForPagination()) {
            m_pageLogicalHeight = 0;
            m_isPaginated = false;
        } else
            m_isPaginated = m_pageLogicalHeight || renderer.enclosingFragmentedFlow();
    }

    // Propagate line grid information.
    if (ancestor)
        propagateLineGridInfo(*ancestor, renderer);

    if (lineGrid() && (lineGrid()->style().writingMode() == renderer.style().writingMode()) && is<RenderMultiColumnFlow>(renderer))
        computeLineGridPaginationOrigin(downcast<RenderMultiColumnFlow>(renderer));

    // If we have a new grid to track, then add it to our set.
    if (renderer.style().lineGrid() != RenderStyle::initialLineGrid() && is<RenderBlockFlow>(renderer))
        establishLineGrid(layoutStateStack, downcast<RenderBlockFlow>(renderer));
}

LayoutUnit RenderLayoutState::pageLogicalOffset(RenderBox* child, LayoutUnit childLogicalOffset) const
{
    if (child->isHorizontalWritingMode())
        return m_layoutOffset.height() + childLogicalOffset - m_pageOffset.height();
    return m_layoutOffset.width() + childLogicalOffset - m_pageOffset.width();
}

void RenderLayoutState::computeLineGridPaginationOrigin(const RenderMultiColumnFlow& multicol)
{
    if (!isPaginated() || !pageLogicalHeight())
        return;

    if (!multicol.progressionIsInline())
        return;
    // We need to cache a line grid pagination origin so that we understand how to reset the line grid
    // at the top of each column.
    // Get the current line grid and offset.
    ASSERT(m_lineGrid);
    // Get the hypothetical line box used to establish the grid.
    auto* lineGridBox = m_lineGrid->lineGridBox();
    if (!lineGridBox)
        return;

    // Now determine our position on the grid. Our baseline needs to be adjusted to the nearest baseline multiple
    // as established by the line box.
    // FIXME: Need to handle crazy line-box-contain values that cause the root line box to not be considered. I assume
    // the grid should honor line-box-contain.
    bool isHorizontalWritingMode = m_lineGrid->isHorizontalWritingMode();
    LayoutUnit lineGridBlockOffset = isHorizontalWritingMode ? m_lineGridOffset.height() : m_lineGridOffset.width();
    LayoutUnit firstLineTopWithLeading = lineGridBlockOffset + lineGridBox->lineBoxTop();
    LayoutUnit pageLogicalTop = isHorizontalWritingMode ? m_pageOffset.height() : m_pageOffset.width();
    if (pageLogicalTop <= firstLineTopWithLeading)
        return;

    // Shift to the next highest line grid multiple past the page logical top. Cache the delta
    // between this new value and the page logical top as the pagination origin.
    auto lineBoxHeight = lineGridBox->lineBoxHeight();
    if (!roundToInt(lineBoxHeight))
        return;
    LayoutUnit remainder = roundToInt(pageLogicalTop - firstLineTopWithLeading) % roundToInt(lineBoxHeight);
    LayoutUnit paginationDelta = lineBoxHeight - remainder;
    if (isHorizontalWritingMode)
        m_lineGridPaginationOrigin.setHeight(paginationDelta);
    else
        m_lineGridPaginationOrigin.setWidth(paginationDelta);
}

void RenderLayoutState::propagateLineGridInfo(const RenderLayoutState& ancestor, RenderBox& renderer)
{
    // Disable line grids for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and
    // writing mode roots.
    if (renderer.isUnsplittableForPagination())
        return;

    m_lineGrid = ancestor.lineGrid();
    m_lineGridOffset = ancestor.lineGridOffset();
    m_lineGridPaginationOrigin = ancestor.lineGridPaginationOrigin();
}

void RenderLayoutState::establishLineGrid(const FrameViewLayoutContext::LayoutStateStack& layoutStateStack, RenderBlockFlow& renderer)
{
    // First check to see if this grid has been established already.
    if (m_lineGrid) {
        if (m_lineGrid->style().lineGrid() == renderer.style().lineGrid())
            return;
        auto* currentGrid = m_lineGrid.get();
        for (int i = layoutStateStack.size() - 1; i >= 0; --i) {
            auto& currentState = *layoutStateStack[i].get();
            if (currentState.m_lineGrid == currentGrid)
                continue;
            currentGrid = currentState.lineGrid();
            if (!currentGrid)
                break;
            if (currentGrid->style().lineGrid() == renderer.style().lineGrid()) {
                m_lineGrid = currentGrid;
                m_lineGridOffset = currentState.m_lineGridOffset;
                return;
            }
        }
    }
    
    // We didn't find an already-established grid with this identifier. Our render object establishes the grid.
    m_lineGrid = renderer;
    m_lineGridOffset = m_layoutOffset;
}

void RenderLayoutState::addLayoutDelta(LayoutSize delta)
{
    m_layoutDelta += delta;
#if ASSERT_ENABLED
    m_layoutDeltaXSaturated |= m_layoutDelta.width() == LayoutUnit::max() || m_layoutDelta.width() == LayoutUnit::min();
    m_layoutDeltaYSaturated |= m_layoutDelta.height() == LayoutUnit::max() || m_layoutDelta.height() == LayoutUnit::min();
#endif
}

#if ASSERT_ENABLED
bool RenderLayoutState::layoutDeltaMatches(LayoutSize delta) const
{
    return (delta.width() == m_layoutDelta.width() || m_layoutDeltaXSaturated) && (delta.height() == m_layoutDelta.height() || m_layoutDeltaYSaturated);
}
#endif

LayoutStateMaintainer::LayoutStateMaintainer(RenderBox& root, LayoutSize offset, bool disablePaintOffsetCache, LayoutUnit pageHeight, bool pageHeightChanged)
    : m_context(root.view().frameView().layoutContext())
    , m_paintOffsetCacheIsDisabled(disablePaintOffsetCache)
{
    m_didPushLayoutState = m_context.pushLayoutState(root, offset, pageHeight, pageHeightChanged);
    if (m_didPushLayoutState && m_paintOffsetCacheIsDisabled)
        m_context.disablePaintOffsetCache();
}

LayoutStateMaintainer::~LayoutStateMaintainer()
{
    if (!m_didPushLayoutState)
        return;
    m_context.popLayoutState();
    if (m_paintOffsetCacheIsDisabled)
        m_context.enablePaintOffsetCache();
}

LayoutStateDisabler::LayoutStateDisabler(FrameViewLayoutContext& context)
    : m_context(context)
{
    m_context.disablePaintOffsetCache();
}

LayoutStateDisabler::~LayoutStateDisabler()
{
    m_context.enablePaintOffsetCache();
}

static bool shouldDisablePaintOffsetCacheForSubtree(RenderElement& subtreeLayoutRoot)
{
    for (auto* renderer = &subtreeLayoutRoot; renderer; renderer = renderer->container()) {
        if (renderer->hasTransform() || renderer->hasReflection())
            return true;
    }
    return false;
}

SubtreeLayoutStateMaintainer::SubtreeLayoutStateMaintainer(RenderElement* subtreeLayoutRoot)
{
    if (subtreeLayoutRoot) {
        m_context = &subtreeLayoutRoot->view().frameView().layoutContext();
        m_context->pushLayoutState(*subtreeLayoutRoot);
        if (shouldDisablePaintOffsetCacheForSubtree(*subtreeLayoutRoot)) {
            m_context->disablePaintOffsetCache();
            m_didDisablePaintOffsetCache = true;
        }
    }
}

SubtreeLayoutStateMaintainer::~SubtreeLayoutStateMaintainer()
{
    if (m_context) {
        m_context->popLayoutState();
        if (m_didDisablePaintOffsetCache)
            m_context->enablePaintOffsetCache();
    }
}

PaginatedLayoutStateMaintainer::PaginatedLayoutStateMaintainer(RenderBlockFlow& flow)
    : m_context(flow.view().frameView().layoutContext())
    , m_pushed(m_context.pushLayoutStateForPaginationIfNeeded(flow))
{
}

PaginatedLayoutStateMaintainer::~PaginatedLayoutStateMaintainer()
{
    if (m_pushed)
        m_context.popLayoutState();
}

} // namespace WebCore

