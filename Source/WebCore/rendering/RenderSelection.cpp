/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RenderSelection.h"

#include "Document.h"
#include "FrameSelection.h"
#include "Highlight.h"
#include "Logging.h"
#include "Position.h"
#include "Range.h"
#include "RenderLayer.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "VisibleSelection.h"
#include <wtf/WeakRef.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

namespace {

struct SelectionContext {
    
    using RendererMap = SingleThreadWeakHashMap<RenderObject, std::unique_ptr<RenderSelectionGeometry>>;
    using RenderBlockMap = SingleThreadWeakHashMap<const RenderBlock, std::unique_ptr<RenderBlockSelectionGeometry>>;

    unsigned startOffset;
    unsigned endOffset;
    RendererMap renderers;
    RenderBlockMap blocks;
};

}

static CheckedPtr<RenderObject> rendererAfterOffset(const RenderObject& renderer, unsigned offset)
{
    CheckedPtr child = renderer.childAt(offset);
    return child ? child : renderer.nextInPreOrderAfterChildren();
}

static bool isValidRendererForSelection(const RenderObject& renderer, const RenderRange& selection)
{
    if (!renderer.containingBlock())
        return false;

    if (renderer.isSkippedContent())
        return false;

    if (renderer.selectionState() == RenderObject::HighlightState::None)
        return false;

    return renderer.canBeSelectionLeaf() || &renderer == selection.start() || &renderer == selection.end();
}

static CheckedPtr<RenderBlock> containingBlockBelowView(const RenderObject& renderer)
{
    CheckedPtr containingBlock = renderer.containingBlock();
    return is<RenderView>(containingBlock) ? nullptr : containingBlock;
}

static SelectionContext collectSelectionData(const RenderRange& selection, bool repaintDifference)
{
    SelectionContext oldSelectionData { selection.startOffset(), selection.endOffset(), { }, { } };
    // Blocks contain selected objects and fill gaps between them, either on the left, right, or in between lines and blocks.
    // In order to get the repaint rect right, we have to examine left, middle, and right rects individually, since otherwise
    // the union of those rects might remain the same even when changes have occurred.
    CheckedPtr start = selection.start();
    CheckedPtr<RenderObject> stop;
    if (CheckedPtr selectionEnd = selection.end())
        stop = rendererAfterOffset(*selectionEnd, selection.endOffset());
    RenderRangeIterator selectionIterator(start.get());
    while (start && start != stop) {
        if (isValidRendererForSelection(*start, selection)) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            oldSelectionData.renderers.set(*start, makeUnique<RenderSelectionGeometry>(*start, true));
            if (repaintDifference) {
                for (CheckedPtr block = containingBlockBelowView(*start); block; block = containingBlockBelowView(*block)) {
                    auto& blockInfo = oldSelectionData.blocks.add(*block, nullptr).iterator->value;
                    if (blockInfo)
                        break;
                    blockInfo = makeUnique<RenderBlockSelectionGeometry>(*block);
                }
            }
        }
        start = selectionIterator.next();
    }
    return oldSelectionData;
}

RenderSelection::RenderSelection(RenderView& view)
    : RenderHighlight(IsSelection)
    , m_renderView(view)
#if ENABLE(SERVICE_CONTROLS)
    , m_selectionGeometryGatherer(view)
#endif
{
}

void RenderSelection::set(const RenderRange& selection, RepaintMode blockRepaintMode)
{
    if ((selection.start() && !selection.end()) || (selection.end() && !selection.start()))
        return;
    // Just return if the selection hasn't changed.
    auto isCaret = m_renderView->frame().selection().isCaret();
    if (selection == m_renderRange && m_selectionWasCaret == isCaret)
        return;
#if ENABLE(SERVICE_CONTROLS)
    // Clear the current rects and create a notifier for the new rects we are about to gather.
    // The Notifier updates the Editor when it goes out of scope and is destroyed.
    auto notifier = m_selectionGeometryGatherer.clearAndCreateNotifier();
#endif
    m_selectionWasCaret = isCaret;
    apply(selection, blockRepaintMode);
}

void RenderSelection::clear()
{
    if (!m_selectionWasCaret)
        m_renderView->checkedLayer()->repaintBlockSelectionGaps();
    set({ }, RenderSelection::RepaintMode::NewMinusOld);
}

void RenderSelection::repaint() const
{
    HashSet<CheckedPtr<RenderBlock>> processedBlocks;
    CheckedPtr<RenderObject> end;
    if (CheckedPtr rangeEnd = m_renderRange.end())
        end = rendererAfterOffset(*rangeEnd, m_renderRange.endOffset());
    RenderRangeIterator highlightIterator(m_renderRange.start());
    for (CheckedPtr renderer = highlightIterator.current(); renderer && renderer != end; renderer = highlightIterator.next()) {
        if (!isValidRendererForSelection(*renderer, m_renderRange))
            continue;
        RenderSelectionGeometry(*renderer, true).repaint();
        // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
        for (CheckedPtr block = containingBlockBelowView(*renderer); block; block = containingBlockBelowView(*block)) {
            if (!processedBlocks.add(block).isNewEntry)
                break;
            RenderSelectionGeometry(*block, true).repaint();
        }
    }
}

IntRect RenderSelection::collectBounds(ClipToVisibleContent clipToVisibleContent) const
{
    LOG_WITH_STREAM(Selection, stream << "SelectionData::collectBounds (clip to visible " << (clipToVisibleContent == ClipToVisibleContent::Yes ? "yes" : "no"));

    SelectionContext::RendererMap renderers;
    CheckedPtr start = m_renderRange.start();
    CheckedPtr<RenderObject> stop;
    if (CheckedPtr rangeEnd = m_renderRange.end())
        stop = rendererAfterOffset(*rangeEnd, m_renderRange.endOffset());

    RenderRangeIterator selectionIterator(start.get());
    while (start && start != stop) {
        if (isValidRendererForSelection(*start, m_renderRange)) {
            // Blocks are responsible for painting line gaps and margin gaps. They must be examined as well.
            renderers.set(*start, makeUnique<RenderSelectionGeometry>(*start, clipToVisibleContent == ClipToVisibleContent::Yes));
            LOG_WITH_STREAM(Selection, stream << " added start " << *start << " with rect " << renderers.get(*start)->rect());

            CheckedPtr block = start->containingBlock();
            while (block && !is<RenderView>(*block)) {
                LOG_WITH_STREAM(Selection, stream << " added block " << *block);
                auto& blockSelectionGeometry = renderers.add(*block, nullptr).iterator->value;
                if (blockSelectionGeometry)
                    break;
                blockSelectionGeometry = makeUnique<RenderSelectionGeometry>(*block, clipToVisibleContent == ClipToVisibleContent::Yes);
                LOG_WITH_STREAM(Selection, stream << " added containing block " << *block << " with rect " << blockSelectionGeometry->rect());
                block = block->containingBlock();
            }
        }
        start = selectionIterator.next();
    }

    // Now create a single bounding box rect that encloses the whole selection.
    LayoutRect selectionRect;
    for (auto slectionEntry : renderers) {
        auto* selectionGeometry = slectionEntry.value.get();
        // RenderSelectionGeometry::rect() is in the coordinates of the repaintContainer, so map to page coordinates.
        LayoutRect currentRect = selectionGeometry->rect();
        if (currentRect.isEmpty())
            continue;

        if (CheckedPtr repaintContainer = selectionGeometry->repaintContainer()) {
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

void RenderSelection::apply(const RenderRange& newSelection, RepaintMode blockRepaintMode)
{
    auto oldSelectionData = collectSelectionData(m_renderRange, blockRepaintMode == RepaintMode::NewXOROld);
    // Remove current selection.
    for (auto selectionEntry : oldSelectionData.renderers) {
        CheckedRef renderer = selectionEntry.key;
        renderer->setSelectionStateIfNeeded(RenderObject::HighlightState::None);
    }
    m_renderRange = newSelection;
    CheckedPtr selectionStart = m_renderRange.start();
    // Update the selection status of all objects between selectionStart and selectionEnd
    if (selectionStart && selectionStart == m_renderRange.end())
        selectionStart->setSelectionStateIfNeeded(RenderObject::HighlightState::Both);
    else {
        if (selectionStart)
            selectionStart->setSelectionStateIfNeeded(RenderObject::HighlightState::Start);
        if (CheckedPtr end = m_renderRange.end())
            end->setSelectionStateIfNeeded(RenderObject::HighlightState::End);
    }

    CheckedPtr<RenderObject> selectionEnd;
    CheckedPtr selectionDataEnd = m_renderRange.end();
    if (selectionDataEnd)
        selectionEnd = rendererAfterOffset(*selectionDataEnd, m_renderRange.endOffset());
    RenderRangeIterator selectionIterator(selectionStart.get());
    for (CheckedPtr currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (currentRenderer == selectionStart || currentRenderer == m_renderRange.end())
            continue;
        if (!currentRenderer->canBeSelectionLeaf() || currentRenderer->isSkippedContent())
            continue;
        currentRenderer->setSelectionStateIfNeeded(RenderObject::HighlightState::Inside);
    }

    if (blockRepaintMode != RepaintMode::Nothing)
        m_renderView->checkedLayer()->clearBlockSelectionGapsBounds();

    // Now that the selection state has been updated for the new objects, walk them again and
    // put them in the new objects list.
    SelectionContext::RendererMap newSelectedRenderers;
    SelectionContext::RenderBlockMap newSelectedBlocks;
    selectionIterator = RenderRangeIterator(selectionStart.get());
    for (CheckedPtr currentRenderer = selectionStart; currentRenderer && currentRenderer != selectionEnd; currentRenderer = selectionIterator.next()) {
        if (isValidRendererForSelection(*currentRenderer, m_renderRange)) {
            auto selectionGeometry = makeUnique<RenderSelectionGeometry>(*currentRenderer, true);
#if ENABLE(SERVICE_CONTROLS)
            for (auto& quad : selectionGeometry->collectedSelectionQuads())
                m_selectionGeometryGatherer.addQuad(selectionGeometry->checkedRepaintContainer().get(), quad);
            if (!currentRenderer->isRenderTextOrLineBreak())
                m_selectionGeometryGatherer.setTextOnly(false);
#endif
            newSelectedRenderers.set(*currentRenderer, WTFMove(selectionGeometry));
            CheckedPtr containingBlock = currentRenderer->containingBlock();
            while (containingBlock && !is<RenderView>(*containingBlock)) {
                auto& blockSelectionGeometry = newSelectedBlocks.add(*containingBlock, nullptr).iterator->value;
                if (blockSelectionGeometry)
                    break;
                blockSelectionGeometry = makeUnique<RenderBlockSelectionGeometry>(*containingBlock);
                containingBlock = containingBlock->containingBlock();
#if ENABLE(SERVICE_CONTROLS)
                m_selectionGeometryGatherer.addGapRects(blockSelectionGeometry->checkedRepaintContainer().get(), blockSelectionGeometry->rects());
#endif
            }
        }
    }

    if (blockRepaintMode == RepaintMode::Nothing)
        return;

    // Have any of the old selected objects changed compared to the new selection?
    for (auto selectedRendererInfo : oldSelectionData.renderers) {
        CheckedRef renderer = selectedRendererInfo.key;
        auto* newInfo = newSelectedRenderers.get(renderer.get());
        auto* oldInfo = selectedRendererInfo.value.get();
        if (!newInfo || oldInfo->rect() != newInfo->rect() || oldInfo->state() != newInfo->state()
            || (m_renderRange.start() == renderer.ptr() && oldSelectionData.startOffset != m_renderRange.startOffset())
            || (m_renderRange.end() == renderer.ptr() && oldSelectionData.endOffset != m_renderRange.endOffset())) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedRenderers.remove(renderer.get());
            }
        }
    }

    // Any new objects that remain were not found in the old objects dict, and so they need to be updated.
    for (auto selectedRendererInfo : newSelectedRenderers)
        selectedRendererInfo.value->repaint();

    // Have any of the old blocks changed?
    for (auto selectedBlockInfo : oldSelectionData.blocks) {
        CheckedRef block = selectedBlockInfo.key;
        auto* newInfo = newSelectedBlocks.get(block.get());
        auto* oldInfo = selectedBlockInfo.value.get();
        if (!newInfo || oldInfo->rects() != newInfo->rects() || oldInfo->state() != newInfo->state()) {
            oldInfo->repaint();
            if (newInfo) {
                newInfo->repaint();
                newSelectedBlocks.remove(block.get());
            }
        }
    }

    // Any new blocks that remain were not found in the old blocks dict, and so they need to be updated.
    for (auto selectedBlockInfo : newSelectedBlocks)
        selectedBlockInfo.value->repaint();
}

} // namespace WebCore
