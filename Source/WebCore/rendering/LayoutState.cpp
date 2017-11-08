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

#include "LayoutContext.h"
#include "RenderFragmentedFlow.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderMultiColumnFlow.h"
#include "RenderView.h"

namespace WebCore {

LayoutState::LayoutState(RenderElement& renderer)
    : m_clipped(false)
    , m_isPaginated(false)
    , m_pageLogicalHeightChanged(false)
#if !ASSERT_DISABLED
    , m_layoutDeltaXSaturated(false)
    , m_layoutDeltaYSaturated(false)
#endif
#ifndef NDEBUG
    , m_renderer(&renderer)
#endif
{
    if (RenderElement* container = renderer.container()) {
        FloatPoint absContentPoint = container->localToAbsolute(FloatPoint(), UseTransforms);
        m_paintOffset = LayoutSize(absContentPoint.x(), absContentPoint.y());

        if (container->hasOverflowClip()) {
            m_clipped = true;
            auto& containerBox = downcast<RenderBox>(*container);
            m_clipRect = LayoutRect(toLayoutPoint(m_paintOffset), containerBox.cachedSizeForOverflowClip());
            m_paintOffset -= toLayoutSize(containerBox.scrollPosition());
        }
    }
}

LayoutState::LayoutState(std::unique_ptr<LayoutState> ancestor, RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged)
    : m_clipped(false)
    , m_isPaginated(false)
    , m_pageLogicalHeightChanged(false)
#if !ASSERT_DISABLED
    , m_layoutDeltaXSaturated(false)
    , m_layoutDeltaYSaturated(false)
#endif
    , m_ancestor(WTFMove(ancestor))
#ifndef NDEBUG
    , m_renderer(&renderer)
#endif
{
    if (m_ancestor) {
        computeOffsets(renderer, offset);
        computeClipRect(renderer);
    }
    computePaginationInformation(renderer, pageLogicalHeight, pageLogicalHeightChanged);
}

void LayoutState::computeOffsets(RenderBox& renderer, LayoutSize offset)
{
    ASSERT(m_ancestor);

    bool fixed = renderer.isFixedPositioned();
    if (fixed) {
        FloatPoint fixedOffset = renderer.view().localToAbsolute(FloatPoint(), IsFixed);
        m_paintOffset = LayoutSize(fixedOffset.x(), fixedOffset.y()) + offset;
    } else
        m_paintOffset = m_ancestor->m_paintOffset + offset;

    if (renderer.isOutOfFlowPositioned() && !fixed) {
        if (auto* container = renderer.container()) {
            if (container->isInFlowPositioned() && is<RenderInline>(*container))
                m_paintOffset += downcast<RenderInline>(*container).offsetForInFlowPositionedInline(&renderer);
        }
    }

    m_layoutOffset = m_paintOffset;

    if (renderer.isInFlowPositioned() && renderer.hasLayer())
        m_paintOffset += renderer.layer()->offsetForInFlowPosition();

    if (renderer.hasOverflowClip())
        m_paintOffset -= toLayoutSize(renderer.scrollPosition());

    m_layoutDelta = m_ancestor->m_layoutDelta;
#if !ASSERT_DISABLED
    m_layoutDeltaXSaturated = m_ancestor->m_layoutDeltaXSaturated;
    m_layoutDeltaYSaturated = m_ancestor->m_layoutDeltaYSaturated;
#endif
}

void LayoutState::computeClipRect(RenderBox& renderer)
{
    ASSERT(m_ancestor);

    m_clipped = !renderer.isFixedPositioned() && m_ancestor->m_clipped;
    if (m_clipped)
        m_clipRect = m_ancestor->m_clipRect;
    if (!renderer.hasOverflowClip())
        return;

    LayoutRect clipRect(toLayoutPoint(m_paintOffset) + renderer.view().frameView().layoutContext().layoutDelta(), renderer.cachedSizeForOverflowClip());
    if (m_clipped)
        m_clipRect.intersect(clipRect);
    else
        m_clipRect = clipRect;
    m_clipped = true;
    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

void LayoutState::computePaginationInformation(RenderBox& renderer, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged)
{
    // If we establish a new page height, then cache the offset to the top of the first page.
    // We can compare this later on to figure out what part of the page we're actually on,
    if (pageLogicalHeight || renderer.isRenderFragmentedFlow()) {
        m_pageLogicalHeight = pageLogicalHeight;
        bool isFlipped = renderer.style().isFlippedBlocksWritingMode();
        m_pageOffset = LayoutSize(m_layoutOffset.width() + (!isFlipped ? renderer.borderLeft() + renderer.paddingLeft() : renderer.borderRight() + renderer.paddingRight()), m_layoutOffset.height() + (!isFlipped ? renderer.borderTop() + renderer.paddingTop() : renderer.borderBottom() + renderer.paddingBottom()));
        m_pageLogicalHeightChanged = pageLogicalHeightChanged;
        m_isPaginated = true;
    } else if (m_ancestor) {
        // If we don't establish a new page height, then propagate the old page height and offset down.
        m_pageLogicalHeight = m_ancestor->m_pageLogicalHeight;
        m_pageLogicalHeightChanged = m_ancestor->m_pageLogicalHeightChanged;
        m_pageOffset = m_ancestor->m_pageOffset;

        // Disable pagination for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and writing mode roots.
        if (renderer.isUnsplittableForPagination()) {
            m_pageLogicalHeight = 0;
            m_isPaginated = false;
        } else
            m_isPaginated = m_pageLogicalHeight || renderer.enclosingFragmentedFlow();
    }

    // Propagate line grid information.
    propagateLineGridInfo(renderer);

    if (lineGrid() && (lineGrid()->style().writingMode() == renderer.style().writingMode()) && is<RenderMultiColumnFlow>(renderer))
        downcast<RenderMultiColumnFlow>(renderer).computeLineGridPaginationOrigin(*this);

    // If we have a new grid to track, then add it to our set.
    if (renderer.style().lineGrid() != RenderStyle::initialLineGrid() && is<RenderBlockFlow>(renderer))
        establishLineGrid(downcast<RenderBlockFlow>(renderer));
}

void LayoutState::clearPaginationInformation()
{
    m_pageLogicalHeight = m_ancestor->m_pageLogicalHeight;
    m_pageOffset = m_ancestor->m_pageOffset;
}

LayoutUnit LayoutState::pageLogicalOffset(RenderBox* child, LayoutUnit childLogicalOffset) const
{
    if (child->isHorizontalWritingMode())
        return m_layoutOffset.height() + childLogicalOffset - m_pageOffset.height();
    return m_layoutOffset.width() + childLogicalOffset - m_pageOffset.width();
}

void LayoutState::propagateLineGridInfo(RenderBox& renderer)
{
    // Disable line grids for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and
    // writing mode roots.
    if (!m_ancestor || renderer.isUnsplittableForPagination())
        return;

    m_lineGrid = m_ancestor->m_lineGrid;
    m_lineGridOffset = m_ancestor->m_lineGridOffset;
    m_lineGridPaginationOrigin = m_ancestor->m_lineGridPaginationOrigin;
}

void LayoutState::establishLineGrid(RenderBlockFlow& renderer)
{
    // First check to see if this grid has been established already.
    if (m_lineGrid) {
        if (m_lineGrid->style().lineGrid() == renderer.style().lineGrid())
            return;
        RenderBlockFlow* currentGrid = m_lineGrid;
        for (LayoutState* currentState = m_ancestor.get(); currentState; currentState = currentState->m_ancestor.get()) {
            if (currentState->m_lineGrid == currentGrid)
                continue;
            currentGrid = currentState->m_lineGrid;
            if (!currentGrid)
                break;
            if (currentGrid->style().lineGrid() == renderer.style().lineGrid()) {
                m_lineGrid = currentGrid;
                m_lineGridOffset = currentState->m_lineGridOffset;
                return;
            }
        }
    }
    
    // We didn't find an already-established grid with this identifier. Our render object establishes the grid.
    m_lineGrid = &renderer;
    m_lineGridOffset = m_layoutOffset;
}

LayoutStateMaintainer::LayoutStateMaintainer(RenderBox& root, LayoutSize offset, bool disablePaintOffsetCache, LayoutUnit pageHeight, bool pageHeightChanged)
    : m_layoutContext(root.view().frameView().layoutContext())
    , m_paintOffsetCacheIsDisabled(disablePaintOffsetCache)
{
    push(root, offset, pageHeight, pageHeightChanged);
}

LayoutStateMaintainer::LayoutStateMaintainer(LayoutContext& layoutContext)
    : m_layoutContext(layoutContext)
{
}

LayoutStateMaintainer::~LayoutStateMaintainer()
{
    // FIXME: Remove conditional push/pop.
    if (m_didCallPush && !m_didCallPop)
        pop();
    ASSERT(!m_didCallPush || m_didCallPush == m_didCallPop);
}

void LayoutStateMaintainer::push(RenderBox& root, LayoutSize offset, LayoutUnit pageHeight, bool pageHeightChanged)
{
    ASSERT(!m_didCallPush);
    m_didCallPush = true;
    // We push state even if disabled, because we still need to store layoutDelta
    m_didPushLayoutState = m_layoutContext.pushLayoutState(root, offset, pageHeight, pageHeightChanged);
    if (!m_didPushLayoutState)
        return;
    if (m_paintOffsetCacheIsDisabled)
        m_layoutContext.disablePaintOffsetCache();
}

void LayoutStateMaintainer::pop()
{
    ASSERT(!m_didCallPop);
    m_didCallPop = true;
    if (!m_didCallPush)
        return;
    if (!m_didPushLayoutState)
        return;
    m_layoutContext.popLayoutState();
    if (m_paintOffsetCacheIsDisabled)
        m_layoutContext.enablePaintOffsetCache();
}

LayoutStateDisabler::LayoutStateDisabler(LayoutContext& layoutContext)
    : m_layoutContext(layoutContext)
{
    m_layoutContext.disablePaintOffsetCache();
}

LayoutStateDisabler::~LayoutStateDisabler()
{
    m_layoutContext.enablePaintOffsetCache();
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
    : m_subtreeLayoutRoot(subtreeLayoutRoot)
{
    if (m_subtreeLayoutRoot) {
        auto& layoutContext = m_subtreeLayoutRoot->view().frameView().layoutContext();
        layoutContext.pushLayoutState(*m_subtreeLayoutRoot);
        if (shouldDisablePaintOffsetCacheForSubtree(*m_subtreeLayoutRoot)) {
            layoutContext.disablePaintOffsetCache();
            m_didDisablePaintOffsetCache = true;
        }
    }
}

SubtreeLayoutStateMaintainer::~SubtreeLayoutStateMaintainer()
{
    if (m_subtreeLayoutRoot) {
        auto& layoutContext = m_subtreeLayoutRoot->view().frameView().layoutContext();
        layoutContext.popLayoutState(*m_subtreeLayoutRoot);
        if (m_didDisablePaintOffsetCache)
            layoutContext.enablePaintOffsetCache();
    }
}

PaginatedLayoutStateMaintainer::PaginatedLayoutStateMaintainer(RenderBlockFlow& flow)
    : m_flow(flow)
    , m_pushed(flow.view().frameView().layoutContext().pushLayoutStateForPaginationIfNeeded(flow))
{
}

PaginatedLayoutStateMaintainer::~PaginatedLayoutStateMaintainer()
{
    if (m_pushed)
        m_flow.view().frameView().layoutContext().popLayoutState(m_flow);
}

} // namespace WebCore

