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
#include "HighlightData.h"

#include "Document.h"
#include "FrameSelection.h"
#include "Logging.h"
#include "Position.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "VisibleSelection.h"
#include <wtf/text/TextStream.h>

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

class HighlightIterator {
public:
    HighlightIterator(RenderObject* start)
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

static bool isValidRendererForSelection(const RenderObject& renderer, const HighlightData::RenderRange& selection)
{
    return (renderer.canBeSelectionLeaf() || &renderer == selection.start() || &renderer == selection.end())
        && renderer.selectionState() != RenderObject::HighlightState::None
        && renderer.containingBlock();
}

static RenderBlock* containingBlockBelowView(const RenderObject& renderer)
{
    auto* containingBlock = renderer.containingBlock();
    return is<RenderView>(containingBlock) ? nullptr : containingBlock;
}

static SelectionData collectSelectionData(const HighlightData::RenderRange& selection, bool repaintDifference)
{
    SelectionData oldSelectionData { selection.startOffset(), selection.endOffset(), { }, { } };
    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    auto* start = selection.start();
    RenderObject* stop = nullptr;
    if (selection.end())
        stop = rendererAfterOffset(*selection.end(), selection.endOffset().value());
    HighlightIterator selectionIterator(start);
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

HighlightData::HighlightData(RenderView& view)
    : m_renderView(view)
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionRectGatherer(view)
#endif
{
}

void HighlightData::setRenderRange(const RenderRange& renderRange)
{
    ASSERT(renderRange.start() && renderRange.end());
    m_renderRange = renderRange;
}

RenderObject::HighlightState HighlightData::highlightStateForRenderer(RenderObject& renderer)
{
    if (&renderer == m_renderRange.start()) {
        if (m_renderRange.start() && m_renderRange.end() && m_renderRange.start() == m_renderRange.end())
            return RenderObject::HighlightState::Both;
        if (m_renderRange.start())
            return RenderObject::HighlightState::Start;
    }
    if (&renderer == m_renderRange.end())
        return RenderObject::HighlightState::End;

    RenderObject* highlightEnd = nullptr;
    auto* highlightDataEnd = m_renderRange.end();
    if (highlightDataEnd)
        highlightEnd = rendererAfterOffset(*highlightDataEnd, m_renderRange.endOffset().value());
    UNUSED_PARAM(highlightEnd);
    HighlightIterator highlightIterator(m_renderRange.start());
    for (auto* currentRenderer = m_renderRange.start(); currentRenderer && currentRenderer != m_renderRange.end(); currentRenderer = highlightIterator.next()) {
        if (currentRenderer == m_renderRange.start() || currentRenderer == m_renderRange.end())
            continue;
        if (!currentRenderer->canBeSelectionLeaf())
            continue;
        if (&renderer == currentRenderer)
            return RenderObject::HighlightState::Inside;
    }
    return RenderObject::HighlightState::None;
}

void HighlightData::setSelection(const RenderRange& selection, RepaintMode blockRepaintMode)
{
    if ((selection.start() && !selection.end()) || (selection.end() && !selection.start()))
        return;
    // Just return if the selection hasn't changed.
    auto isCaret = m_renderView.frame().selection().isCaret();
    if (selection == m_renderRange && m_selectionWasCaret == isCaret)
        return;
#if ENABLE(SERVICE_CONTROLS)
    // Clear the current rects and create a notifier for the new rects we are about to gather.
    // The Notifier updates the Editor when it goes out of scope and is destroyed.
    auto rectNotifier = m_selectionRectGatherer.clearAndCreateNotifier();
#endif
    m_selectionWasCaret = isCaret;
    applySelection(selection, blockRepaintMode);
}

void HighlightData::clearSelection()
{
    m_renderView.layer()->repaintBlockSelectionGaps();
    setSelection({ }, HighlightData::RepaintMode::NewMinusOld);
}

void HighlightData::repaintSelection() const
{
    HashSet<RenderBlock*> processedBlocks;
    RenderObject* end = nullptr;
    if (m_renderRange.end())
        end = rendererAfterOffset(*m_renderRange.end(), m_renderRange.endOffset().value());
    HighlightIterator highlightIterator(m_renderRange.start());
    for (auto* renderer = highlightIterator.current(); renderer && renderer != end; renderer = highlightIterator.next()) {
        if (!renderer->canBeSelectionLeaf() && renderer != m_renderRange.start() && renderer != m_renderRange.end())
            continue;
        if (renderer->selectionState() == RenderObject::HighlightState::None)
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

IntRect HighlightData::collectBounds(ClipToVisibleContent clipToVisibleContent) const
{
    LOG_WITH_STREAM(Selection, stream << "HighlightData::collectBounds (clip to visible " << (clipToVisibleContent == ClipToVisibleContent::Yes ? "yes" : "no"));

    SelectionData::RendererMap renderers;
    auto* start = m_renderRange.start();
    RenderObject* stop = nullptr;
    if (m_renderRange.end())
        stop = rendererAfterOffset(*m_renderRange.end(), m_renderRange.endOffset().value());

    HighlightIterator highlightIterator(start);
    while (start && start != stop) {
        if ((start->canBeSelectionLeaf() || start == m_renderRange.start() || start == m_renderRange.end())
            && start->selectionState() != RenderObject::HighlightState::None) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            renderers.set(start, makeUnique<RenderSelectionInfo>(*start, clipToVisibleContent == ClipToVisibleContent::Yes));
            LOG_WITH_STREAM(Selection, stream << " added start " << *start << " with rect " << renderers.get(start)->rect());

            auto* block = start->containingBlock();
            while (block && !is<RenderView>(*block)) {
                LOG_WITH_STREAM(Scrolling, stream << " added block " << *block);
                std::unique_ptr<RenderSelectionInfo>& blockInfo = renderers.add(block, nullptr).iterator->value;
                if (blockInfo)
                    break;
                blockInfo = makeUnique<RenderSelectionInfo>(*block, clipToVisibleContent == ClipToVisibleContent::Yes);
                LOG_WITH_STREAM(Selection, stream << " added containing block " << *block << " with rect " << blockInfo->rect());
                block = block->containingBlock();
            }
        }
        start = highlightIterator.next();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selectionRect;
    for (auto& info : renderers.values()) {
        // RenderSelectionInfo::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currentRect = info->rect();
        if (currentRect.isEmpty())
            continue;

        if (auto* repaintContainer = info->repaintContainer()) {
            FloatRect localRect = currentRect;
            FloatQuad absQuad = repaintContainer->localToAbsoluteQuad(localRect);
            currentRect = absQuad.enclosingBoundingBox();
            LOG_WITH_STREAM(Selection, stream << " rect " << localRect << " mapped to " << currentRect << " in container " << *repaintContainer);
        }
        selectionRect.unite(currentRect);
    }

    LOG_WITH_STREAM(Selection, stream << " final rect " << selectionRect);
    return snappedIntRect(selectionRect);
}

void HighlightData::applySelection(const RenderRange& newSelection, RepaintMode blockRepaintMode)
{
    auto oldSelectionData = collectSelectionData(m_renderRange, blockRepaintMode == RepaintMode::NewXOROld);
    // Remove current selection.
    for (auto* renderer : oldSelectionData.renderers.keys())
        renderer->setSelectionStateIfNeeded(RenderObject::HighlightState::None);
    m_renderRange = newSelection;
    auto* selectionStart = m_renderRange.start();
    // Update the selection status of all objects between selectionStart and selectionEnd
    if (selectionStart && selectionStart == m_renderRange.end())
        selectionStart->setSelectionStateIfNeeded(RenderObject::HighlightState::Both);
    else {
        if (selectionStart)
            selectionStart->setSelectionStateIfNeeded(RenderObject::HighlightState::Start);
        if (auto* end = m_renderRange.end())
            end->setSelectionStateIfNeeded(RenderObject::HighlightState::End);
    }

    RenderObject* selectionEnd = nullptr;
    auto* selectionDataEnd = m_renderRange.end();
    if (selectionDataEnd)
        selectionEnd = rendererAfterOffset(*selectionDataEnd, m_renderRange.endOffset().value());
    HighlightIterator selectionIterator(selectionStart);
    for (auto* currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (currentRenderer == selectionStart || currentRenderer == m_renderRange.end())
            continue;
        if (!currentRenderer->canBeSelectionLeaf())
            continue;
        currentRenderer->setSelectionStateIfNeeded(RenderObject::HighlightState::Inside);
    }

    if (blockRepaintMode != RepaintMode::Nothing)
        m_renderView.layer()->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    SelectionData::RendererMap newSelectedRenderers;
    SelectionData::RenderBlockMap newSelectedBlocks;
    selectionIterator = HighlightIterator(selectionStart);
    for (auto* currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (isValidRendererForSelection(*currentRenderer, m_renderRange)) {
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
            || (m_renderRange.start() == renderer && oldSelectionData.startOffset != m_renderRange.startOffset())
            || (m_renderRange.end() == renderer && oldSelectionData.endOffset != m_renderRange.endOffset())) {
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
