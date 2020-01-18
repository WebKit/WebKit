/*
 * Copyright (C) 2014 Igalia S.L.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "SelectionRangeData.h"

#include "Document.h"
#include "FrameSelection.h"
#include "Position.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "VisibleSelection.h"

namespace WebCore {

namespace { // See bug #177808.

struct SelectionData {
    using RendererMap = HashMap<RenderObject*, std::unique_ptr<RenderSelectionInfo>>;
    using RenderBlockMap = HashMap<const RenderBlock*, std::unique_ptr<RenderBlockSelectionInfo>>;

    Optional<unsigned> startOffset;
    Optional<unsigned> endOffset;
    RendererMap renderers;
    RenderBlockMap blocks;
};

class SelectionIterator {
public:
    SelectionIterator(RenderObject* start)
        : m_current(start)
    {
        checkForSpanner();
    }
    
    RenderObject* current() const
    {
        return m_current;
    }
    
    RenderObject* next()
    {
        RenderObject* currentSpan = m_spannerStack.isEmpty() ? nullptr : m_spannerStack.last()->spanner();
        m_current = m_current->nextInPreOrder(currentSpan);
        checkForSpanner();
        if (!m_current && currentSpan) {
            RenderObject* placeholder = m_spannerStack.last();
            m_spannerStack.removeLast();
            m_current = placeholder->nextInPreOrder();
            checkForSpanner();
        }
        return m_current;
    }

private:
    void checkForSpanner()
    {
        if (!is<RenderMultiColumnSpannerPlaceholder>(m_current))
            return;
        auto& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*m_current);
        m_spannerStack.append(&placeholder);
        m_current = placeholder.spanner();
    }

    RenderObject* m_current { nullptr };
    Vector<RenderMultiColumnSpannerPlaceholder*> m_spannerStack;
};

} // anonymous namespace

static RenderObject* rendererAfterOffset(const RenderObject& renderer, unsigned offset)
{
    auto* child = renderer.childAt(offset);
    return child ? child : renderer.nextInPreOrderAfterChildren();
}

static bool isValidRendererForSelection(const RenderObject& renderer, const SelectionRangeData::Context& selection)
{
    return (renderer.canBeSelectionLeaf() || &renderer == selection.start() || &renderer == selection.end())
        && renderer.selectionState() != RenderObject::SelectionNone
        && renderer.containingBlock();
}

static RenderBlock* containingBlockBelowView(const RenderObject& renderer)
{
    auto* containingBlock = renderer.containingBlock();
    return is<RenderView>(containingBlock) ? nullptr : containingBlock;
}

static SelectionData collect(const SelectionRangeData::Context& selection, bool repaintDifference)
{
    SelectionData oldSelectionData { selection.startOffset(), selection.endOffset(), { }, { } };
    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    auto* start = selection.start();
    RenderObject* stop = nullptr;
    if (selection.end())
        stop = rendererAfterOffset(*selection.end(), selection.endOffset().value());
    SelectionIterator selectionIterator(start);
    while (start && start != stop) {
        if (isValidRendererForSelection(*start, selection)) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            oldSelectionData.renderers.set(start, makeUnique<RenderSelectionInfo>(*start, true));
            if (repaintDifference) {
                for (auto* block = containingBlockBelowView(*start); block; block = containingBlockBelowView(*block)) {
                    auto& blockInfo = oldSelectionData.blocks.add(block, nullptr).iterator->value;
                    if (blockInfo)
                        break;
                    blockInfo = makeUnique<RenderBlockSelectionInfo>(*block);
                }
            }
        }
        start = selectionIterator.next();
    }
    return oldSelectionData;
}

SelectionRangeData::SelectionRangeData(RenderView& view)
    : m_renderView(view)
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionRectGatherer(view)
#endif
{
}

void SelectionRangeData::setContext(const Context& context)
{
    ASSERT(context.start() && context.end());
    m_selectionContext = context;
}

RenderObject::SelectionState SelectionRangeData::selectionStateForRenderer(RenderObject& renderer)
{
    // FIXME: we shouldln't have to check that a renderer is a descendant of the render node
    // from the range. This is likely because we aren't using VisiblePositions yet.
    // Planned fix in a followup: <rdar://problem/58095923>
    // https://bugs.webkit.org/show_bug.cgi?id=205529
    
    if (&renderer == m_selectionContext.start()) {
        if (m_selectionContext.start() && m_selectionContext.end() && m_selectionContext.start() == m_selectionContext.end())
            return RenderObject::SelectionBoth;
        if (m_selectionContext.start())
            return RenderObject::SelectionStart;
    }
    if (&renderer == m_selectionContext.end())
        return RenderObject::SelectionEnd;

    RenderObject* selectionEnd = nullptr;
    auto* selectionDataEnd = m_selectionContext.end();
    if (selectionDataEnd)
        selectionEnd = rendererAfterOffset(*selectionDataEnd, m_selectionContext.endOffset().value());
    SelectionIterator selectionIterator(m_selectionContext.start());
    for (auto* currentRenderer = m_selectionContext.start(); currentRenderer && currentRenderer != m_selectionContext.end(); currentRenderer = selectionIterator.next()) {
        if (currentRenderer == m_selectionContext.start() || currentRenderer == m_selectionContext.end())
            continue;
        if (!currentRenderer->canBeSelectionLeaf())
            continue;
        if (&renderer == currentRenderer)
            return RenderObject::SelectionInside;
    }
    return RenderObject::SelectionNone;
    
}

void SelectionRangeData::set(const Context& selection, RepaintMode blockRepaintMode)
{
    if ((selection.start() && !selection.end()) || (selection.end() && !selection.start()))
        return;
    // Just return if the selection hasn't changed.
    auto isCaret = m_renderView.frame().selection().isCaret();
    if (selection == m_selectionContext && m_selectionWasCaret == isCaret)
        return;
#if ENABLE(SERVICE_CONTROLS)
    // Clear the current rects and create a notifier for the new rects we are about to gather.
    // The Notifier updates the Editor when it goes out of scope and is destroyed.
    auto rectNotifier = m_selectionRectGatherer.clearAndCreateNotifier();
#endif
    m_selectionWasCaret = isCaret;
    apply(selection, blockRepaintMode);
}

void SelectionRangeData::clear()
{
    m_renderView.layer()->repaintBlockSelectionGaps();
    set({ }, SelectionRangeData::RepaintMode::NewMinusOld);
}

void SelectionRangeData::repaint() const
{
    HashSet<RenderBlock*> processedBlocks;
    RenderObject* end = nullptr;
    if (m_selectionContext.end())
        end = rendererAfterOffset(*m_selectionContext.end(), m_selectionContext.endOffset().value());
    SelectionIterator selectionIterator(m_selectionContext.start());
    for (auto* renderer = selectionIterator.current(); renderer && renderer != end; renderer = selectionIterator.next()) {
        if (!renderer->canBeSelectionLeaf() && renderer != m_selectionContext.start() && renderer != m_selectionContext.end())
            continue;
        if (renderer->selectionState() == RenderObject::SelectionNone)
            continue;
        RenderSelectionInfo(*renderer, true).repaint();
        // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
        for (auto* block = containingBlockBelowView(*renderer); block; block = containingBlockBelowView(*block)) {
            if (!processedBlocks.add(block).isNewEntry)
                break;
            RenderSelectionInfo(*block, true).repaint();
        }
    }
}

IntRect SelectionRangeData::collectBounds(ClipToVisibleContent clipToVisibleContent) const
{
    SelectionData::RendererMap renderers;
    auto* start = m_selectionContext.start();
    RenderObject* stop = nullptr;
    if (m_selectionContext.end())
        stop = rendererAfterOffset(*m_selectionContext.end(), m_selectionContext.endOffset().value());
    SelectionIterator selectionIterator(start);
    while (start && start != stop) {
        if ((start->canBeSelectionLeaf() || start == m_selectionContext.start() || start == m_selectionContext.end())
            && start->selectionState() != RenderObject::SelectionNone) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            renderers.set(start, makeUnique<RenderSelectionInfo>(*start, clipToVisibleContent == ClipToVisibleContent::Yes));
            auto* block = start->containingBlock();
            while (block && !is<RenderView>(*block)) {
                std::unique_ptr<RenderSelectionInfo>& blockInfo = renderers.add(block, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = makeUnique<RenderSelectionInfo>(*block, clipToVisibleContent == ClipToVisibleContent::Yes);
                block = block->containingBlock();
            }
        }
        start = selectionIterator.next();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selectionRect;
    for (auto& info : renderers.values()) {
        // RenderSelectionInfo::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currentRect = info->rect();
        if (auto* repaintContainer = info->repaintContainer()) {
            FloatQuad absQuad = repaintContainer->localToAbsoluteQuad(FloatRect(currentRect));
            currentRect = absQuad.enclosingBoundingBox();
        }
        selectionRect.unite(currentRect);
    }
    return snappedIntRect(selectionRect);
}

void SelectionRangeData::apply(const Context& newSelection, RepaintMode blockRepaintMode)
{
    auto oldSelectionData = collect(m_selectionContext, blockRepaintMode == RepaintMode::NewXOROld);
    // Remove current selection.
    for (auto* renderer : oldSelectionData.renderers.keys())
        renderer->setSelectionStateIfNeeded(RenderObject::SelectionNone);
    m_selectionContext = newSelection;
    auto* selectionStart = m_selectionContext.start();
    // Update the selection status of all objects between selectionStart and selectionEnd
    if (selectionStart && selectionStart == m_selectionContext.end())
        selectionStart->setSelectionStateIfNeeded(RenderObject::SelectionBoth);
    else {
        if (selectionStart)
            selectionStart->setSelectionStateIfNeeded(RenderObject::SelectionStart);
        if (auto* end = m_selectionContext.end())
            end->setSelectionStateIfNeeded(RenderObject::SelectionEnd);
    }

    RenderObject* selectionEnd = nullptr;
    auto* selectionDataEnd = m_selectionContext.end();
    if (selectionDataEnd)
        selectionEnd = rendererAfterOffset(*selectionDataEnd, m_selectionContext.endOffset().value());
    SelectionIterator selectionIterator(selectionStart);
    for (auto* currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (currentRenderer == selectionStart || currentRenderer == m_selectionContext.end())
            continue;
        if (!currentRenderer->canBeSelectionLeaf())
            continue;
        currentRenderer->setSelectionStateIfNeeded(RenderObject::SelectionInside);
    }

    if (blockRepaintMode != RepaintMode::Nothing)
        m_renderView.layer()->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    SelectionData::RendererMap newSelectedRenderers;
    SelectionData::RenderBlockMap newSelectedBlocks;
    selectionIterator = SelectionIterator(selectionStart);
    for (auto* currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (isValidRendererForSelection(*currentRenderer, m_selectionContext)) {
            std::unique_ptr<RenderSelectionInfo> selectionInfo = makeUnique<RenderSelectionInfo>(*currentRenderer, true);
#if ENABLE(SERVICE_CONTROLS)
            for (auto& rect : selectionInfo->collectedSelectionRects())
                m_selectionRectGatherer.addRect(selectionInfo->repaintContainer(), rect);
            if (!currentRenderer->isTextOrLineBreak())
                m_selectionRectGatherer.setTextOnly(false);
#endif
            newSelectedRenderers.set(currentRenderer, WTFMove(selectionInfo));
            auto* containingBlock = currentRenderer->containingBlock();
            while (containingBlock && !is<RenderView>(*containingBlock)) {
                std::unique_ptr<RenderBlockSelectionInfo>& blockInfo = newSelectedBlocks.add(containingBlock, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = makeUnique<RenderBlockSelectionInfo>(*containingBlock);
                containingBlock = containingBlock->containingBlock();
#if ENABLE(SERVICE_CONTROLS)
                m_selectionRectGatherer.addGapRects(blockInfo->repaintContainer(), blockInfo->rects());
#endif
            }
        }
    }

    if (blockRepaintMode == RepaintMode::Nothing)
        return;

    // Have any of the old selected objects changed compared to the new selection?
    for (auto& selectedRendererInfo : oldSelectionData.renderers) {
        auto* renderer = selectedRendererInfo.key;
        auto* newInfo = newSelectedRenderers.get(renderer);
        auto* oldInfo = selectedRendererInfo.value.get();
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state()
            || (m_selectionContext.start() == renderer && oldSelectionData.startOffset != m_selectionContext.startOffset())
            || (m_selectionContext.end() == renderer && oldSelectionData.endOffset != m_selectionContext.endOffset())) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedRenderers.remove(renderer);
            }
        }
    }

    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    for (auto& selectedRendererInfo : newSelectedRenderers)
        selectedRendererInfo.value->repaint();

    // Have any of the old blocks changed?
    for (auto& selectedBlockInfo : oldSelectionData.blocks) {
        auto* block = selectedBlockInfo.key;
        auto* newInfo = newSelectedBlocks.get(block);
        auto* oldInfo = selectedBlockInfo.value.get();
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedBlocks.remove(block);
            }
        }
    }

    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    for (auto& selectedBlockInfo : newSelectedBlocks)
        selectedBlockInfo.value->repaint();
}

} // namespace WebCore
