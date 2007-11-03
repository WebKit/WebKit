/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RootInlineBox.h"

#include "BidiResolver.h"
#include "Document.h"
#include "EllipsisBox.h"
#include "Frame.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "RenderArena.h"
#include "RenderBlock.h"

using namespace std;

namespace WebCore {
    
typedef WTF::HashMap<const RootInlineBox*, EllipsisBox*> EllipsisBoxMap;
static EllipsisBoxMap* gEllipsisBoxMap = 0;

void* RootInlineBox::Overflow::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RootInlineBox::Overflow::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RootInlineBox::Overflow::destroy(RenderArena* renderArena)
{
    delete this;
    // Recover the size left there for us by operator delete and free the memory.
    renderArena->free(*(size_t *)this, this);
}

void RootInlineBox::destroy(RenderArena* arena)
{
    if (m_overflow)
        m_overflow->destroy(arena);
    detachEllipsisBox(arena);
    InlineFlowBox::destroy(arena);
}

void RootInlineBox::detachEllipsisBox(RenderArena* arena)
{
    if (m_hasEllipsisBox) {
        EllipsisBox* box = gEllipsisBoxMap->take(this);
        box->setParent(0);
        box->destroy(arena);
        m_hasEllipsisBox = false;
    }
}

void RootInlineBox::clearTruncation()
{
    if (m_hasEllipsisBox) {
        detachEllipsisBox(m_object->renderArena());
        InlineFlowBox::clearTruncation();
    }
}

bool RootInlineBox::canAccommodateEllipsis(bool ltr, int blockEdge, int lineBoxEdge, int ellipsisWidth)
{
    // First sanity-check the unoverflowed width of the whole line to see if there is sufficient room.
    int delta = ltr ? lineBoxEdge - blockEdge : blockEdge - lineBoxEdge;
    if (width() - delta < ellipsisWidth)
        return false;

    // Next iterate over all the line boxes on the line.  If we find a replaced element that intersects
    // then we refuse to accommodate the ellipsis.  Otherwise we're ok.
    return InlineFlowBox::canAccommodateEllipsis(ltr, blockEdge, ellipsisWidth);
}

void RootInlineBox::placeEllipsis(const AtomicString& ellipsisStr,  bool ltr, int blockEdge, int ellipsisWidth,
                                  InlineBox* markupBox)
{
    // Create an ellipsis box.
    EllipsisBox* ellipsisBox = new (m_object->renderArena()) EllipsisBox(m_object, ellipsisStr, this,
                                                              ellipsisWidth - (markupBox ? markupBox->width() : 0),
                                                              yPos(), height(), baseline(), !prevRootBox(),
                                                              markupBox);
    
    if (!gEllipsisBoxMap)
        gEllipsisBoxMap = new EllipsisBoxMap();
    gEllipsisBoxMap->add(this, ellipsisBox);
    m_hasEllipsisBox = true;

    if (ltr && (xPos() + width() + ellipsisWidth) <= blockEdge) {
        ellipsisBox->m_x = xPos() + width();
        return;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph.  Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    ellipsisBox->m_x = placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
}

int RootInlineBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    int result = InlineFlowBox::placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
    if (result == -1)
        result = ltr ? blockEdge - ellipsisWidth : blockEdge;
    return result;
}

void RootInlineBox::paintEllipsisBox(RenderObject::PaintInfo& paintInfo, int tx, int ty) const
{
    if (m_hasEllipsisBox && object()->shouldPaintWithinRoot(paintInfo) && object()->style()->visibility() == VISIBLE &&
            paintInfo.phase == PaintPhaseForeground)
        ellipsisBox()->paint(paintInfo, tx, ty);
}

#if PLATFORM(MAC)
void RootInlineBox::addHighlightOverflow()
{
    // Highlight acts as a selection inflation.
    FloatRect rootRect(0, selectionTop(), width(), selectionHeight());
    IntRect inflatedRect = enclosingIntRect(object()->document()->frame()->customHighlightLineRect(object()->style()->highlight(), rootRect, object()->node()));
    setHorizontalOverflowPositions(min(leftOverflow(), inflatedRect.x()), max(rightOverflow(), inflatedRect.right()));
    setVerticalOverflowPositions(min(topOverflow(), inflatedRect.y()), max(bottomOverflow(), inflatedRect.bottom()));
}

void RootInlineBox::paintCustomHighlight(RenderObject::PaintInfo& paintInfo, int tx, int ty, const AtomicString& highlightType)
{
    if (!object()->shouldPaintWithinRoot(paintInfo) || object()->style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseForeground)
        return;

    // Get the inflated rect so that we can properly hit test.
    FloatRect rootRect(tx + xPos(), ty + selectionTop(), width(), selectionHeight());
    FloatRect inflatedRect = object()->document()->frame()->customHighlightLineRect(highlightType, rootRect, object()->node());
    if (inflatedRect.intersects(paintInfo.rect))
        object()->document()->frame()->paintCustomHighlight(highlightType, rootRect, rootRect, false, true, object()->node());
}
#endif

void RootInlineBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    InlineFlowBox::paint(paintInfo, tx, ty);
    paintEllipsisBox(paintInfo, tx, ty);
#if PLATFORM(MAC)
    RenderStyle* styleToUse = object()->style(m_firstLine);
    if (styleToUse->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
        paintCustomHighlight(paintInfo, tx, ty, styleToUse->highlight());
#endif
}

bool RootInlineBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    if (m_hasEllipsisBox && object()->style()->visibility() == VISIBLE) {
        if (ellipsisBox()->nodeAtPoint(request, result, x, y, tx, ty)) {
            object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
            return true;
        }
    }
    return InlineFlowBox::nodeAtPoint(request, result, x, y, tx, ty);
}

void RootInlineBox::adjustPosition(int dx, int dy)
{
    InlineFlowBox::adjustPosition(dx, dy);
    if (m_overflow) {
        m_overflow->m_topOverflow += dy;
        m_overflow->m_bottomOverflow += dy;
        m_overflow->m_selectionTop += dy;
        m_overflow->m_selectionBottom += dy;
    }
    m_blockHeight += dy;
}

void RootInlineBox::childRemoved(InlineBox* box)
{
    if (box->object() == m_lineBreakObj)
        setLineBreakInfo(0, 0, BidiStatus());

    for (RootInlineBox* prev = prevRootBox(); prev && prev->lineBreakObj() == box->object(); prev = prev->prevRootBox()) {
        prev->setLineBreakInfo(0, 0, BidiStatus());
        prev->markDirty();
    }
}

GapRects RootInlineBox::fillLineSelectionGap(int selTop, int selHeight, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty,
                                             const RenderObject::PaintInfo* paintInfo)
{
    RenderObject::SelectionState lineState = selectionState();

    bool leftGap, rightGap;
    block()->getHorizontalSelectionGapInfo(lineState, leftGap, rightGap);

    GapRects result;

    InlineBox* firstBox = firstSelectedBox();
    InlineBox* lastBox = lastSelectedBox();
    if (leftGap)
        result.uniteLeft(block()->fillLeftSelectionGap(firstBox->parent()->object(),
                                                       firstBox->xPos(), selTop, selHeight,
                                                       rootBlock, blockX, blockY, tx, ty, paintInfo));
    if (rightGap)
        result.uniteRight(block()->fillRightSelectionGap(lastBox->parent()->object(),
                                                         lastBox->xPos() + lastBox->width(), selTop, selHeight,
                                                         rootBlock, blockX, blockY, tx, ty, paintInfo));

    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        int lastX = firstBox->xPos() + firstBox->width();
        for (InlineBox* box = firstBox->nextLeafChild(); box; box = box->nextLeafChild()) {
            if (box->selectionState() != RenderObject::SelectionNone) {
                result.uniteCenter(block()->fillHorizontalSelectionGap(box->parent()->object(),
                                                                       lastX + tx, selTop + ty,
                                                                       box->xPos() - lastX, selHeight, paintInfo));
                lastX = box->xPos() + box->width();
            }
            if (box == lastBox)
                break;
        }
    }

    return result;
}

void RootInlineBox::setHasSelectedChildren(bool b)
{
    if (m_hasSelectedChildren == b)
        return;
    m_hasSelectedChildren = b;
}

RenderObject::SelectionState RootInlineBox::selectionState()
{
    // Walk over all of the selected boxes.
    RenderObject::SelectionState state = RenderObject::SelectionNone;
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        RenderObject::SelectionState boxState = box->selectionState();
        if ((boxState == RenderObject::SelectionStart && state == RenderObject::SelectionEnd) ||
            (boxState == RenderObject::SelectionEnd && state == RenderObject::SelectionStart))
            state = RenderObject::SelectionBoth;
        else if (state == RenderObject::SelectionNone ||
                 ((boxState == RenderObject::SelectionStart || boxState == RenderObject::SelectionEnd) &&
                  (state == RenderObject::SelectionNone || state == RenderObject::SelectionInside)))
            state = boxState;
        if (state == RenderObject::SelectionBoth)
            break;
    }

    return state;
}

InlineBox* RootInlineBox::firstSelectedBox()
{
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

InlineBox* RootInlineBox::lastSelectedBox()
{
    for (InlineBox* box = lastLeafChild(); box; box = box->prevLeafChild()) {
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    }

    return 0;
}

int RootInlineBox::selectionTop()
{
    int selectionTop = m_overflow ? m_overflow->m_selectionTop : m_y;
    if (!prevRootBox())
        return selectionTop;

    int prevBottom = prevRootBox()->selectionBottom();
    if (prevBottom < selectionTop && block()->containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the previous
        // line's bottom overflow if the offsets are greater on both sides.
        int prevLeft = block()->leftOffset(prevBottom);
        int prevRight = block()->rightOffset(prevBottom);
        int newLeft = block()->leftOffset(selectionTop);
        int newRight = block()->rightOffset(selectionTop);
        if (prevLeft > newLeft || prevRight < newRight)
            return selectionTop;
    }

    return prevBottom;
}

RenderBlock* RootInlineBox::block() const
{
    return static_cast<RenderBlock*>(m_object);
}

bool isEditableLeaf(InlineBox* leaf)
{
    return leaf && leaf->object() && leaf->object()->element() && leaf->object()->element()->isContentEditable();
}

InlineBox* RootInlineBox::closestLeafChildForXPos(int x, bool onlyEditableLeaves)
{
    InlineBox* firstLeaf = firstLeafChildAfterBox();
    InlineBox* lastLeaf = lastLeafChildBeforeBox();
    if (firstLeaf == lastLeaf && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        return firstLeaf;

    // Avoid returning a list marker when possible.
    if (x <= firstLeaf->m_x && !firstLeaf->object()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(firstLeaf)))
        // The x coordinate is less or equal to left edge of the firstLeaf.
        // Return it.
        return firstLeaf;

    if (x >= lastLeaf->m_x + lastLeaf->m_width && !lastLeaf->object()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(lastLeaf)))
        // The x coordinate is greater or equal to right edge of the lastLeaf.
        // Return it.
        return lastLeaf;

    InlineBox* closestLeaf = 0;
    for (InlineBox* leaf = firstLeaf; leaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->object()->isListMarker() && (!onlyEditableLeaves || isEditableLeaf(leaf))) {
            closestLeaf = leaf;
            if (x < leaf->m_x + leaf->m_width)
                // The x coordinate is less than the right edge of the box.
                // Return it.
                return leaf;
        }
    }

    return closestLeaf ? closestLeaf : lastLeaf;
}

BidiStatus RootInlineBox::lineBreakBidiStatus() const
{ 
    return BidiStatus(m_lineBreakBidiStatusEor, m_lineBreakBidiStatusLastStrong, m_lineBreakBidiStatusLast, m_lineBreakContext);
}

void RootInlineBox::setLineBreakInfo(RenderObject* obj, unsigned breakPos, const BidiStatus& status)
{
    m_lineBreakObj = obj;
    m_lineBreakPos = breakPos;
    m_lineBreakBidiStatusEor = status.eor;
    m_lineBreakBidiStatusLastStrong = status.lastStrong;
    m_lineBreakBidiStatusLast = status.last;
    m_lineBreakContext = status.context;
}

EllipsisBox* RootInlineBox::ellipsisBox() const
{
    if (!m_hasEllipsisBox)
        return false;
    return gEllipsisBoxMap->get(this);
}

void RootInlineBox::setVerticalOverflowPositions(int top, int bottom) 
{ 
    if (!m_overflow) {
        if (top == m_y && bottom == m_y + m_height)
            return;
        m_overflow = new (m_object->renderArena()) Overflow(this);
    }
    m_overflow->m_topOverflow = top; 
    m_overflow->m_bottomOverflow = bottom; 
}

} // namespace WebCore
