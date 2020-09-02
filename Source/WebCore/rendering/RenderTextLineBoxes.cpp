/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "RenderTextLineBoxes.h"

#include "EllipsisBox.h"
#include "InlineTextBox.h"
#include "RenderBlock.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "RootInlineBox.h"

namespace WebCore {

RenderTextLineBoxes::RenderTextLineBoxes()
    : m_first(nullptr)
    , m_last(nullptr)
{
}

InlineTextBox* RenderTextLineBoxes::createAndAppendLineBox(RenderText& renderText)
{
    auto textBox = renderText.createTextBox();
    if (!m_first) {
        m_first = textBox.get();
        m_last = textBox.get();
    } else {
        m_last->setNextTextBox(textBox.get());
        textBox->setPreviousTextBox(m_last);
        m_last = textBox.get();
    }
    return textBox.release();
}

void RenderTextLineBoxes::extract(InlineTextBox& box)
{
    checkConsistency();

    m_last = box.prevTextBox();
    if (&box == m_first)
        m_first = nullptr;
    if (box.prevTextBox())
        box.prevTextBox()->setNextTextBox(nullptr);
    box.setPreviousTextBox(nullptr);
    for (auto* current = &box; current; current = current->nextTextBox())
        current->setExtracted();

    checkConsistency();
}

void RenderTextLineBoxes::attach(InlineTextBox& box)
{
    checkConsistency();

    if (m_last) {
        m_last->setNextTextBox(&box);
        box.setPreviousTextBox(m_last);
    } else
        m_first = &box;
    InlineTextBox* last = nullptr;
    for (auto* current = &box; current; current = current->nextTextBox()) {
        current->setExtracted(false);
        last = current;
    }
    m_last = last;

    checkConsistency();
}

void RenderTextLineBoxes::remove(InlineTextBox& box)
{
    checkConsistency();

    if (&box == m_first)
        m_first = box.nextTextBox();
    if (&box == m_last)
        m_last = box.prevTextBox();
    if (box.nextTextBox())
        box.nextTextBox()->setPreviousTextBox(box.prevTextBox());
    if (box.prevTextBox())
        box.prevTextBox()->setNextTextBox(box.nextTextBox());

    checkConsistency();
}

void RenderTextLineBoxes::removeAllFromParent(RenderText& renderer)
{
    if (!m_first) {
        if (renderer.parent())
            renderer.parent()->dirtyLinesFromChangedChild(renderer);
        return;
    }
    for (auto* box = m_first; box; box = box->nextTextBox())
        box->removeFromParent();
}

void RenderTextLineBoxes::deleteAll()
{
    if (!m_first)
        return;
    InlineTextBox* next;
    for (auto* current = m_first; current; current = next) {
        next = current->nextTextBox();
        delete current;
    }
    m_first = nullptr;
    m_last = nullptr;
}

InlineTextBox* RenderTextLineBoxes::findNext(int offset, int& position) const
{
    if (!m_first)
        return nullptr;
    // FIXME: This looks buggy. The function is only used for debugging purposes.
    auto current = m_first;
    int currentOffset = current->len();
    while (offset > currentOffset && current->nextTextBox()) {
        current = current->nextTextBox();
        currentOffset = current->start() + current->len();
    }
    // we are now in the correct text run
    position = (offset > currentOffset ? current->len() : current->len() - (currentOffset - offset));
    return current;
}

LayoutRect RenderTextLineBoxes::visualOverflowBoundingBox(const RenderText& renderer) const
{
    if (!m_first)
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    auto logicalLeftSide = LayoutUnit::max();
    auto logicalRightSide = LayoutUnit::min();
    for (auto* current = m_first; current; current = current->nextTextBox()) {
        logicalLeftSide = std::min(logicalLeftSide, current->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, current->logicalRightVisualOverflow());
    }
    
    auto logicalTop = m_first->logicalTopVisualOverflow();
    auto logicalWidth = logicalRightSide - logicalLeftSide;
    auto logicalHeight = m_last->logicalBottomVisualOverflow() - logicalTop;
    
    LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
    if (!renderer.style().isHorizontalWritingMode())
        rect = rect.transposedRect();
    return rect;
}

enum ShouldAffinityBeDownstream { AlwaysDownstream, AlwaysUpstream, UpstreamIfPositionIsNotAtStart };

static bool lineDirectionPointFitsInBox(int pointLineDirection, const InlineTextBox& box, ShouldAffinityBeDownstream& shouldAffinityBeDownstream)
{
    shouldAffinityBeDownstream = AlwaysDownstream;

    // the x coordinate is equal to the left edge of this box
    // the affinity must be downstream so the position doesn't jump back to the previous line
    // except when box is the first box in the line
    if (pointLineDirection <= box.logicalLeft()) {
        shouldAffinityBeDownstream = !box.previousLeafOnLine() ? UpstreamIfPositionIsNotAtStart : AlwaysDownstream;
        return true;
    }

#if !PLATFORM(IOS_FAMILY)
    // and the x coordinate is to the left of the right edge of this box
    // check to see if position goes in this box
    if (pointLineDirection < box.logicalRight()) {
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }
#endif

    // box is first on line
    // and the x coordinate is to the left of the first text box left edge
    if (!box.previousLeafOnLineIgnoringLineBreak() && pointLineDirection < box.logicalLeft())
        return true;

    if (!box.nextLeafOnLineIgnoringLineBreak()) {
        // box is last on line
        // and the x coordinate is to the right of the last text box right edge
        // generate VisiblePosition, use UPSTREAM affinity if possible
        shouldAffinityBeDownstream = UpstreamIfPositionIsNotAtStart;
        return true;
    }

    return false;
}

static VisiblePosition createVisiblePositionForBox(const InlineBox& box, int offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
{
    auto affinity = VisiblePosition::defaultAffinity;
    switch (shouldAffinityBeDownstream) {
    case AlwaysDownstream:
        affinity = Affinity::Downstream;
        break;
    case AlwaysUpstream:
        affinity = Affinity::Upstream;
        break;
    case UpstreamIfPositionIsNotAtStart:
        affinity = offset > box.caretMinOffset() ? Affinity::Upstream : Affinity::Downstream;
        break;
    }
    return box.renderer().createVisiblePosition(offset, affinity);
}

static VisiblePosition createVisiblePositionAfterAdjustingOffsetForBiDi(const InlineTextBox& box, int offset, ShouldAffinityBeDownstream shouldAffinityBeDownstream)
{
    ASSERT(offset >= 0);

    if (offset && static_cast<unsigned>(offset) < box.len())
        return createVisiblePositionForBox(box, box.start() + offset, shouldAffinityBeDownstream);

    bool positionIsAtStartOfBox = !offset;
    if (positionIsAtStartOfBox == box.isLeftToRightDirection()) {
        // offset is on the left edge

        const InlineBox* prevBox = box.previousLeafOnLineIgnoringLineBreak();
        if ((prevBox && prevBox->bidiLevel() == box.bidiLevel())
            || box.renderer().containingBlock()->style().direction() == box.direction()) // FIXME: left on 12CBA
            return createVisiblePositionForBox(box, box.caretLeftmostOffset(), shouldAffinityBeDownstream);

        if (prevBox && prevBox->bidiLevel() > box.bidiLevel()) {
            // e.g. left of B in aDC12BAb
            const InlineBox* leftmostBox;
            do {
                leftmostBox = prevBox;
                prevBox = leftmostBox->previousLeafOnLineIgnoringLineBreak();
            } while (prevBox && prevBox->bidiLevel() > box.bidiLevel());
            return createVisiblePositionForBox(*leftmostBox, leftmostBox->caretRightmostOffset(), shouldAffinityBeDownstream);
        }

        if (!prevBox || prevBox->bidiLevel() < box.bidiLevel()) {
            // e.g. left of D in aDC12BAb
            const InlineBox* rightmostBox;
            const InlineBox* nextBox = &box;
            do {
                rightmostBox = nextBox;
                nextBox = rightmostBox->nextLeafOnLineIgnoringLineBreak();
            } while (nextBox && nextBox->bidiLevel() >= box.bidiLevel());
            return createVisiblePositionForBox(*rightmostBox,
                box.isLeftToRightDirection() ? rightmostBox->caretMaxOffset() : rightmostBox->caretMinOffset(), shouldAffinityBeDownstream);
        }

        return createVisiblePositionForBox(box, box.caretRightmostOffset(), shouldAffinityBeDownstream);
    }

    const InlineBox* nextBox = box.nextLeafOnLineIgnoringLineBreak();
    if ((nextBox && nextBox->bidiLevel() == box.bidiLevel())
        || box.renderer().containingBlock()->style().direction() == box.direction())
        return createVisiblePositionForBox(box, box.caretRightmostOffset(), shouldAffinityBeDownstream);

    // offset is on the right edge
    if (nextBox && nextBox->bidiLevel() > box.bidiLevel()) {
        // e.g. right of C in aDC12BAb
        const InlineBox* rightmostBox;
        do {
            rightmostBox = nextBox;
            nextBox = rightmostBox->nextLeafOnLineIgnoringLineBreak();
        } while (nextBox && nextBox->bidiLevel() > box.bidiLevel());
        return createVisiblePositionForBox(*rightmostBox, rightmostBox->caretLeftmostOffset(), shouldAffinityBeDownstream);
    }

    if (!nextBox || nextBox->bidiLevel() < box.bidiLevel()) {
        // e.g. right of A in aDC12BAb
        const InlineBox* leftmostBox;
        const InlineBox* prevBox = &box;
        do {
            leftmostBox = prevBox;
            prevBox = leftmostBox->previousLeafOnLineIgnoringLineBreak();
        } while (prevBox && prevBox->bidiLevel() >= box.bidiLevel());
        return createVisiblePositionForBox(*leftmostBox,
            box.isLeftToRightDirection() ? leftmostBox->caretMinOffset() : leftmostBox->caretMaxOffset(), shouldAffinityBeDownstream);
    }

    return createVisiblePositionForBox(box, box.caretLeftmostOffset(), shouldAffinityBeDownstream);
}

VisiblePosition RenderTextLineBoxes::positionForPoint(const RenderText& renderer, const LayoutPoint& point) const
{
    if (!m_first || !renderer.text().length())
        return renderer.createVisiblePosition(0, DOWNSTREAM);

    LayoutUnit pointLineDirection = m_first->isHorizontal() ? point.x() : point.y();
    LayoutUnit pointBlockDirection = m_first->isHorizontal() ? point.y() : point.x();
    bool blocksAreFlipped = renderer.style().isFlippedBlocksWritingMode();

    InlineTextBox* lastBox = nullptr;
    for (auto* box = m_first; box; box = box->nextTextBox()) {
        if (box->isLineBreak() && !box->previousLeafOnLine() && box->nextLeafOnLine() && !box->nextLeafOnLine()->isLineBreak())
            box = box->nextTextBox();

        auto& rootBox = box->root();
        LayoutUnit top = std::min(rootBox.selectionTop(RootInlineBox::ForHitTesting::Yes), rootBox.lineTop());
        if (pointBlockDirection > top || (!blocksAreFlipped && pointBlockDirection == top)) {
            LayoutUnit bottom = rootBox.selectionBottom();
            if (rootBox.nextRootBox())
                bottom = std::min(bottom, rootBox.nextRootBox()->lineTop());

            if (pointBlockDirection < bottom || (blocksAreFlipped && pointBlockDirection == bottom)) {
                ShouldAffinityBeDownstream shouldAffinityBeDownstream;
#if PLATFORM(IOS_FAMILY)
                if (pointLineDirection != box->logicalLeft() && point.x() < box->x() + box->logicalWidth()) {
                    int half = box->x() + box->logicalWidth() / 2;
                    auto affinity = point.x() < half ? Affinity::Downstream : Affinity::Upstream;
                    return renderer.createVisiblePosition(box->offsetForPosition(pointLineDirection) + box->start(), affinity);
                }
#endif
                if (lineDirectionPointFitsInBox(pointLineDirection, *box, shouldAffinityBeDownstream))
                    return createVisiblePositionAfterAdjustingOffsetForBiDi(*box, box->offsetForPosition(pointLineDirection), shouldAffinityBeDownstream);
            }
        }
        lastBox = box;
    }

    if (lastBox) {
        ShouldAffinityBeDownstream shouldAffinityBeDownstream;
        lineDirectionPointFitsInBox(pointLineDirection, *lastBox, shouldAffinityBeDownstream);
        return createVisiblePositionAfterAdjustingOffsetForBiDi(*lastBox, lastBox->offsetForPosition(pointLineDirection) + lastBox->start(), shouldAffinityBeDownstream);
    }
    return renderer.createVisiblePosition(0, DOWNSTREAM);
}

void RenderTextLineBoxes::setSelectionState(RenderText& renderer, RenderObject::HighlightState state)
{
    if (state == RenderObject::HighlightState::Inside || state == RenderObject::HighlightState::None) {
        for (auto* box = m_first; box; box = box->nextTextBox())
            box->root().setHasSelectedChildren(state == RenderObject::HighlightState::Inside);
        return;
    }

    auto start = renderer.view().selection().startOffset();
    auto end = renderer.view().selection().endOffset();
    if (state == RenderObject::HighlightState::Start) {
        end = renderer.text().length();
        // to handle selection from end of text to end of line
        if (start && start == end)
            start = end - 1;
    } else if (state == RenderObject::HighlightState::End)
        start = 0;

    for (auto* box = m_first; box; box = box->nextTextBox()) {
        if (box->isSelected(start, end))
            box->root().setHasSelectedChildren(true);
    }
}

static IntRect ellipsisRectForBox(const InlineTextBox& box, unsigned start, unsigned end)
{
    unsigned short truncation = box.truncation();
    if (truncation == cNoTruncation)
        return IntRect();

    auto ellipsis = box.root().ellipsisBox();
    if (!ellipsis)
        return IntRect();
    
    IntRect rect;
    int ellipsisStartPosition = std::max<int>(start - box.start(), 0);
    int ellipsisEndPosition = std::min<int>(end - box.start(), box.len());
    
    // The ellipsis should be considered to be selected if the end of
    // the selection is past the beginning of the truncation and the
    // beginning of the selection is before or at the beginning of the truncation.
    if (ellipsisEndPosition < truncation && ellipsisStartPosition > truncation)
        return IntRect();
    return ellipsis->selectionRect();
}

LayoutRect RenderTextLineBoxes::selectionRectForRange(unsigned start, unsigned end)
{
    LayoutRect rect;
    for (auto* box = m_first; box; box = box->nextTextBox()) {
        rect.unite(box->localSelectionRect(start, end));
        rect.unite(ellipsisRectForBox(*box, start, end));
    }
    return rect;
}

void RenderTextLineBoxes::collectSelectionRectsForRange(unsigned start, unsigned end, Vector<LayoutRect>& rects)
{
    for (auto* box = m_first; box; box = box->nextTextBox()) {
        LayoutRect rect;
        rect.unite(box->localSelectionRect(start, end));
        rect.unite(ellipsisRectForBox(*box, start, end));
        if (!rect.size().isEmpty())
            rects.append(rect);
    }
}

static FloatRect localQuadForTextBox(const InlineTextBox& box, unsigned start, unsigned end, bool useSelectionHeight)
{
    unsigned realEnd = std::min(box.end(), end);
    LayoutRect boxSelectionRect = box.localSelectionRect(start, realEnd);
    if (!boxSelectionRect.height())
        return FloatRect();
    if (useSelectionHeight)
        return boxSelectionRect;
    // Change the height and y position (or width and x for vertical text)
    // because selectionRect uses selection-specific values.
    if (box.isHorizontal()) {
        boxSelectionRect.setHeight(box.height());
        boxSelectionRect.setY(box.y());
    } else {
        boxSelectionRect.setWidth(box.width());
        boxSelectionRect.setX(box.x());
    }
    return boxSelectionRect;
}

Vector<FloatQuad> RenderTextLineBoxes::absoluteQuadsForRange(const RenderText& renderer, unsigned start, unsigned end, bool useSelectionHeight, bool ignoreEmptyTextSelections, bool* wasFixed) const
{
    Vector<FloatQuad> quads;
    for (auto* box = m_first; box; box = box->nextTextBox()) {
        if (ignoreEmptyTextSelections && !box->isSelected(start, end))
            continue;
        if (start <= box->start() && box->end() <= end) {
            FloatRect boundaries = box->calculateBoundaries();
            if (useSelectionHeight) {
                LayoutRect selectionRect = box->localSelectionRect(start, end);
                if (box->isHorizontal()) {
                    boundaries.setHeight(selectionRect.height());
                    boundaries.setY(selectionRect.y());
                } else {
                    boundaries.setWidth(selectionRect.width());
                    boundaries.setX(selectionRect.x());
                }
            }
            quads.append(renderer.localToAbsoluteQuad(boundaries, UseTransforms, wasFixed));
            continue;
        }
        FloatRect rect = localQuadForTextBox(*box, start, end, useSelectionHeight);
        if (!rect.isZero())
            quads.append(renderer.localToAbsoluteQuad(rect, UseTransforms, wasFixed));
    }
    return quads;
}

Vector<IntRect> RenderTextLineBoxes::absoluteRectsForRange(const RenderText& renderer, unsigned start, unsigned end, bool useSelectionHeight, bool* wasFixed) const
{
    return absoluteQuadsForRange(renderer, start, end, useSelectionHeight, false /* ignoreEmptyTextSelections */, wasFixed).map([](auto& quad) { return quad.enclosingBoundingBox(); });
}

Vector<FloatQuad> RenderTextLineBoxes::absoluteQuads(const RenderText& renderer, bool* wasFixed, ClippingOption option) const
{
    Vector<FloatQuad> quads;
    for (auto* box = m_first; box; box = box->nextTextBox()) {
        FloatRect boundaries = box->calculateBoundaries();

        // Shorten the width of this text box if it ends in an ellipsis.
        // FIXME: ellipsisRectForBox should switch to return FloatRect soon with the subpixellayout branch.
        IntRect ellipsisRect = (option == ClipToEllipsis) ? ellipsisRectForBox(*box, 0, renderer.text().length()) : IntRect();
        if (!ellipsisRect.isEmpty()) {
            if (renderer.style().isHorizontalWritingMode())
                boundaries.setWidth(ellipsisRect.maxX() - boundaries.x());
            else
                boundaries.setHeight(ellipsisRect.maxY() - boundaries.y());
        }
        quads.append(renderer.localToAbsoluteQuad(boundaries, UseTransforms, wasFixed));
    }
    return quads;
}

void RenderTextLineBoxes::dirtyAll()
{
    for (auto* box = m_first; box; box = box->nextTextBox())
        box->dirtyLineBoxes();
}

bool RenderTextLineBoxes::dirtyRange(RenderText& renderer, unsigned start, unsigned end, int lengthDelta)
{
    RootInlineBox* firstRootBox = nullptr;
    RootInlineBox* lastRootBox = nullptr;

    // Dirty all text boxes that include characters in between offset and offset+len.
    bool dirtiedLines = false;
    for (auto* current = m_first; current; current = current->nextTextBox()) {
        // FIXME: This shouldn't rely on the end of a dirty line box. See https://bugs.webkit.org/show_bug.cgi?id=97264
        // Text run is entirely before the affected range.
        if (current->end() <= start)
            continue;
        // Text run is entirely after the affected range.
        if (current->start() >= end) {
            current->offsetRun(lengthDelta);
            auto& rootBox = current->root();
            if (!firstRootBox) {
                firstRootBox = &rootBox;
                if (!dirtiedLines) {
                    // The affected area was in between two runs. Mark the root box of the run after the affected area as dirty.
                    firstRootBox->markDirty();
                    dirtiedLines = true;
                }
            }
            lastRootBox = &rootBox;
            continue;
        }
        if (current->end() > start && current->end() <= end) {
            // Text run overlaps with the left end of the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
        if (current->start() <= start && current->end() >= end) {
            // Text run subsumes the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
        if (current->start() < end && current->end() >= end) {
            // Text run overlaps with right end of the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
    }

    // Now we have to walk all of the clean lines and adjust their cached line break information
    // to reflect our updated offsets.
    if (lastRootBox)
        lastRootBox = lastRootBox->nextRootBox();
    if (firstRootBox) {
        auto previousRootBox = firstRootBox->prevRootBox();
        if (previousRootBox)
            firstRootBox = previousRootBox;
    } else if (m_last) {
        ASSERT(!lastRootBox);
        firstRootBox = &m_last->root();
        firstRootBox->markDirty();
        dirtiedLines = true;
    }

    for (auto* current = firstRootBox; current && current != lastRootBox; current = current->nextRootBox()) {
        auto lineBreakPos = current->lineBreakPos();
        if (current->lineBreakObj() == &renderer && (lineBreakPos > end || (start != end && lineBreakPos == end)))
            current->setLineBreakPos(current->lineBreakPos() + lengthDelta);
    }

    // If the text node is empty, dirty the line where new text will be inserted.
    if (!m_first && renderer.parent()) {
        renderer.parent()->dirtyLinesFromChangedChild(renderer);
        dirtiedLines = true;
    }
    return dirtiedLines;
}

inline void RenderTextLineBoxes::checkConsistency() const
{
#if ASSERT_ENABLED
#ifdef CHECK_CONSISTENCY
    const InlineTextBox* prev = nullptr;
    for (auto* child = m_first; child; child = child->nextTextBox()) {
        ASSERT(child->renderer() == this);
        ASSERT(child->prevTextBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_last);
#endif
#endif // ASSERT_ENABLED
}

#if ASSERT_ENABLED
RenderTextLineBoxes::~RenderTextLineBoxes()
{
    ASSERT(!m_first);
    ASSERT(!m_last);
}
#endif

#if !ASSERT_WITH_SECURITY_IMPLICATION_DISABLED
void RenderTextLineBoxes::invalidateParentChildLists()
{
    for (auto* box = m_first; box; box = box->nextTextBox())
        box->invalidateParentChildList();
}
#endif

}
