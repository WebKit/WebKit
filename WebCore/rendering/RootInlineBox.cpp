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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "RootInlineBox.h"

#include "EllipsisBox.h"
#include "RenderBlock.h"

using namespace std;

namespace WebCore {

void RootInlineBox::destroy(RenderArena* arena)
{
    detachEllipsisBox(arena);
    InlineFlowBox::destroy(arena);
}

void RootInlineBox::detachEllipsisBox(RenderArena* arena)
{
    if (m_ellipsisBox) {
        m_ellipsisBox->destroy(arena);
        m_ellipsisBox = 0;
    }
}

void RootInlineBox::clearTruncation()
{
    if (m_ellipsisBox) {
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
    m_ellipsisBox = new (m_object->renderArena()) EllipsisBox(m_object, ellipsisStr, this, 
                                                              ellipsisWidth - (markupBox ? markupBox->width() : 0),
                                                              yPos(), height(), baseline(), !prevRootBox(),
                                                              markupBox);

    if (ltr && (xPos() + width() + ellipsisWidth) <= blockEdge) {
        m_ellipsisBox->m_x = xPos() + width();
        return;
    }

    // Now attempt to find the nearest glyph horizontally and place just to the right (or left in RTL)
    // of that glyph.  Mark all of the objects that intersect the ellipsis box as not painting (as being
    // truncated).
    bool foundBox = false;
    m_ellipsisBox->m_x = placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
}

int RootInlineBox::placeEllipsisBox(bool ltr, int blockEdge, int ellipsisWidth, bool& foundBox)
{
    int result = InlineFlowBox::placeEllipsisBox(ltr, blockEdge, ellipsisWidth, foundBox);
    if (result == -1)
        result = ltr ? blockEdge - ellipsisWidth : blockEdge;
    return result;
}

void RootInlineBox::paintEllipsisBox(RenderObject::PaintInfo& i, int _tx, int _ty) const
{
    if (m_ellipsisBox && object()->shouldPaintWithinRoot(i) && object()->style()->visibility() == VISIBLE &&
        i.phase == PaintPhaseForeground)
        m_ellipsisBox->paint(i, _tx, _ty);
}

void RootInlineBox::paint(RenderObject::PaintInfo& i, int tx, int ty)
{
    InlineFlowBox::paint(i, tx, ty);
    paintEllipsisBox(i, tx, ty);
}

bool RootInlineBox::nodeAtPoint(RenderObject::NodeInfo& i, int x, int y, int tx, int ty)
{
    if (m_ellipsisBox && object()->style()->visibility() == VISIBLE) {
        if (m_ellipsisBox->nodeAtPoint(i, x, y, tx, ty)) {
            object()->setInnerNode(i);
            return true;
        }
    }
    return InlineFlowBox::nodeAtPoint(i, x, y, tx, ty);
}

void RootInlineBox::adjustPosition(int dx, int dy)
{
    InlineFlowBox::adjustPosition(dx, dy);
    m_topOverflow += dy;
    m_bottomOverflow += dy;
    m_blockHeight += dy;
    m_selectionTop += dy;
    m_selectionBottom += dy;
}

void RootInlineBox::childRemoved(InlineBox* box)
{
    if (box->object() == m_lineBreakObj)
        setLineBreakInfo(0, 0, 0, 0);

    RootInlineBox* prev = prevRootBox();
    if (prev && prev->lineBreakObj() == box->object()) {
        prev->setLineBreakInfo(0, 0, 0, 0);
        prev->markDirty();
    }
}

GapRects RootInlineBox::fillLineSelectionGap(int selTop, int selHeight, RenderBlock* rootBlock, int blockX, int blockY, int tx, int ty, 
                                             const RenderObject::PaintInfo* i)
{
    GapRects result;
    RenderObject::SelectionState lineState = selectionState();

    bool leftGap, rightGap;
    block()->getHorizontalSelectionGapInfo(lineState, leftGap, rightGap);

    InlineBox* firstBox = firstSelectedBox();
    InlineBox* lastBox = lastSelectedBox();
    if (leftGap)
        result.uniteLeft(block()->fillLeftSelectionGap(firstBox->parent()->object(), 
                                                       firstBox->xPos(), selTop, selHeight, 
                                                       rootBlock, blockX, blockY, tx, ty, i));
    if (rightGap)
        result.uniteRight(block()->fillRightSelectionGap(lastBox->parent()->object(), 
                                                         lastBox->xPos() + lastBox->width(), selTop, selHeight, 
                                                         rootBlock, blockX, blockY, tx, ty, i));

    if (firstBox && firstBox != lastBox) {
        // Now fill in any gaps on the line that occurred between two selected elements.
        int lastX = firstBox->xPos() + firstBox->width();
        for (InlineBox* box = firstBox->nextLeafChild(); box; box = box->nextLeafChild()) {
            if (box->selectionState() != RenderObject::SelectionNone) {
                result.uniteCenter(block()->fillHorizontalSelectionGap(box->parent()->object(),
                                                                       lastX + tx, selTop + ty,
                                                                       box->xPos() - lastX, selHeight, i));
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
    for (InlineBox* box = firstLeafChild(); box; box = box->nextLeafChild())
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    return 0;
}

InlineBox* RootInlineBox::lastSelectedBox()
{
    for (InlineBox* box = lastLeafChild(); box; box = box->prevLeafChild())
        if (box->selectionState() != RenderObject::SelectionNone)
            return box;
    return 0;
}

int RootInlineBox::selectionTop()
{
    if (!prevRootBox())
        return m_selectionTop;
    
    int prevBottom = prevRootBox()->selectionBottom();
    if (prevBottom < m_selectionTop && block()->containsFloats()) {
        // This line has actually been moved further down, probably from a large line-height, but possibly because the
        // line was forced to clear floats.  If so, let's check the offsets, and only be willing to use the previous
        // line's bottom overflow if the offsets are greater on both sides.
        int prevLeft = block()->leftOffset(prevBottom);
        int prevRight = block()->rightOffset(prevBottom);
        int newLeft = block()->leftOffset(m_selectionTop);
        int newRight = block()->rightOffset(m_selectionTop);
        if (prevLeft > newLeft || prevRight < newRight)
            return m_selectionTop;
    }
    
    return prevBottom;
}

RenderBlock* RootInlineBox::block() const
{
    return static_cast<RenderBlock*>(m_object);
}

InlineBox* RootInlineBox::closestLeafChildForXPos(int _x, int _tx)
{
    InlineBox *firstLeaf = firstLeafChildAfterBox();
    InlineBox *lastLeaf = lastLeafChildBeforeBox();
    if (firstLeaf == lastLeaf)
        return firstLeaf;
    
    // Avoid returning a list marker when possible.
    if (_x <= _tx + firstLeaf->m_x && !firstLeaf->object()->isListMarker())
        // The x coordinate is less or equal to left edge of the firstLeaf.
        // Return it.
        return firstLeaf;
    
    if (_x >= _tx + lastLeaf->m_x + lastLeaf->m_width && !lastLeaf->object()->isListMarker())
        // The x coordinate is greater or equal to right edge of the lastLeaf.
        // Return it.
        return lastLeaf;

    for (InlineBox *leaf = firstLeaf; leaf && leaf != lastLeaf; leaf = leaf->nextLeafChild()) {
        if (!leaf->object()->isListMarker()) {
            int leafX = _tx + leaf->m_x;
            if (_x < leafX + leaf->m_width)
                // The x coordinate is less than the right edge of the box.
                // Return it.
                return leaf;
        }
    }

    return lastLeaf;
}

void RootInlineBox::setLineBreakInfo(RenderObject* obj, unsigned breakPos, BidiStatus* status, BidiContext* context)
{
    m_lineBreakObj = obj;
    m_lineBreakPos = breakPos;
    m_lineBreakContext = context;
    if (status)
        m_lineBreakBidiStatus = *status;
}

}
