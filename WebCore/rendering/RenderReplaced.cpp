/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderReplaced.h"

#include "GraphicsContext.h"
#include "RenderBlock.h"
#include "RenderLayer.h"

using namespace std;

namespace WebCore {

typedef WTF::HashMap<const RenderReplaced*, IntRect> OverflowRectMap;
static OverflowRectMap* gOverflowRectMap = 0;

RenderReplaced::RenderReplaced(Node* node)
    : RenderBox(node)
    , m_intrinsicSize(300, 150)
    , m_selectionState(SelectionNone)
    , m_hasOverflow(false)
{
    setReplaced(true);
}

RenderReplaced::RenderReplaced(Node* node, const IntSize& intrinsicSize)
    : RenderBox(node)
    , m_intrinsicSize(intrinsicSize)
    , m_selectionState(SelectionNone)
    , m_hasOverflow(false)
{
    setReplaced(true);
}

RenderReplaced::~RenderReplaced()
{
    if (m_hasOverflow)
        gOverflowRectMap->remove(this);
}
    
void RenderReplaced::layout()
{
    ASSERT(needsLayout());
    
    IntRect oldBounds;
    IntRect oldOutlineBox;
    bool checkForRepaint = checkForRepaintDuringLayout();
    if (checkForRepaint) {
        oldBounds = absoluteClippedOverflowRect();
        oldOutlineBox = absoluteOutlineBox();
    }
    
    m_height = minimumReplacedHeight();
    
    calcWidth();
    calcHeight();
    adjustOverflowForBoxShadow();
    
    if (checkForRepaint)
        repaintAfterLayoutIfNeeded(oldBounds, oldOutlineBox);
    
    setNeedsLayout(false);
}
    
void RenderReplaced::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaint(paintInfo, tx, ty))
        return;
    
    tx += m_x;
    ty += m_y;
    
    if (hasBoxDecorations() && (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseSelection)) 
        paintBoxDecorations(paintInfo, tx, ty);
    
    if ((paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline) && style()->outlineWidth() && style()->visibility() == VISIBLE)
        paintOutline(paintInfo.context, tx, ty, width(), height(), style());
    
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection)
        return;
    
    if (!shouldPaintWithinRoot(paintInfo))
        return;
    
    bool drawSelectionTint = selectionState() != SelectionNone && !document()->printing();
    if (paintInfo.phase == PaintPhaseSelection) {
        if (selectionState() == SelectionNone)
            return;
        drawSelectionTint = false;
    }

    paintReplaced(paintInfo, tx, ty);
    
    if (drawSelectionTint)
        paintInfo.context->fillRect(selectionRect(), selectionBackgroundColor());
}

bool RenderReplaced::shouldPaint(PaintInfo& paintInfo, int& tx, int& ty)
{
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseOutline && paintInfo.phase != PaintPhaseSelfOutline 
            && paintInfo.phase != PaintPhaseSelection)
        return false;

    if (!shouldPaintWithinRoot(paintInfo))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style()->visibility() != VISIBLE)
        return false;

    int currentTX = tx + m_x;
    int currentTY = ty + m_y;

    // Early exit if the element touches the edges.
    int top = currentTY + overflowTop();
    int bottom = currentTY + overflowHeight();
    if (isSelected() && m_inlineBoxWrapper) {
        int selTop = ty + m_inlineBoxWrapper->root()->selectionTop();
        int selBottom = ty + selTop + m_inlineBoxWrapper->root()->selectionHeight();
        top = min(selTop, top);
        bottom = max(selBottom, bottom);
    }
    
    int os = 2 * maximalOutlineSize(paintInfo.phase);
    if (currentTX + overflowLeft() >= paintInfo.rect.right() + os || currentTX + overflowWidth() <= paintInfo.rect.x() - os)
        return false;
    if (top >= paintInfo.rect.bottom() + os || bottom <= paintInfo.rect.y() - os)
        return false;

    return true;
}

void RenderReplaced::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    int width = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();
    if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent())) {
        m_minPrefWidth = 0;
        m_maxPrefWidth = width;
    } else
        m_minPrefWidth = m_maxPrefWidth = width;

    setPrefWidthsDirty(false);
}

short RenderReplaced::lineHeight(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

short RenderReplaced::baselinePosition(bool, bool) const
{
    return height() + marginTop() + marginBottom();
}

int RenderReplaced::caretMinOffset() const 
{ 
    return 0; 
}

// Returns 1 since a replaced element can have the caret positioned 
// at its beginning (0), or at its end (1).
// NOTE: Yet, "select" elements can have any number of "option" elements
// as children, so this "0 or 1" idea does not really hold up.
int RenderReplaced::caretMaxOffset() const 
{ 
    return 1; 
}

unsigned RenderReplaced::caretMaxRenderedOffset() const
{
    return 1; 
}

VisiblePosition RenderReplaced::positionForCoordinates(int x, int y)
{
    InlineBox* box = inlineBoxWrapper();
    if (!box)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    // FIXME: This code is buggy if the replaced element is relative positioned.

    RootInlineBox* root = box->root();

    int top = root->topOverflow();
    int bottom = root->nextRootBox() ? root->nextRootBox()->topOverflow() : root->bottomOverflow();

    if (y + yPos() < top)
        return VisiblePosition(element(), caretMinOffset(), DOWNSTREAM); // coordinates are above
    
    if (y + yPos() >= bottom)
        return VisiblePosition(element(), caretMaxOffset(), DOWNSTREAM); // coordinates are below
    
    if (element()) {
        if (x <= width() / 2)
            return VisiblePosition(element(), 0, DOWNSTREAM);
        return VisiblePosition(element(), 1, DOWNSTREAM);
    }

    return RenderBox::positionForCoordinates(x, y);
}

IntRect RenderReplaced::selectionRect(bool clipToVisibleContent)
{
    ASSERT(!needsLayout());

    if (!isSelected())
        return IntRect();
    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return absoluteBoundingBoxRect();

    RenderBlock* cb =  containingBlock();
    if (!cb)
        return IntRect();
    
    RootInlineBox* root = m_inlineBoxWrapper->root();
    IntRect rect(0, root->selectionTop() - yPos(), width(), root->selectionHeight());
    
    if (clipToVisibleContent)
        computeAbsoluteRepaintRect(rect);
    else {
        int absx, absy;
        absolutePositionForContent(absx, absy);
        rect.move(absx, absy);
    }
    
    return rect;
}

void RenderReplaced::setSelectionState(SelectionState s)
{
    m_selectionState = s;
    if (m_inlineBoxWrapper) {
        RootInlineBox* line = m_inlineBoxWrapper->root();
        if (line)
            line->setHasSelectedChildren(isSelected());
    }
    
    containingBlock()->setSelectionState(s);
}

bool RenderReplaced::isSelected() const
{
    SelectionState s = selectionState();
    if (s == SelectionNone)
        return false;
    if (s == SelectionInside)
        return true;

    int selectionStart, selectionEnd;
    selectionStartEnd(selectionStart, selectionEnd);
    if (s == SelectionStart)
        return selectionStart == 0;
        
    int end = element()->hasChildNodes() ? element()->childNodeCount() : 1;
    if (s == SelectionEnd)
        return selectionEnd == end;
    if (s == SelectionBoth)
        return selectionStart == 0 && selectionEnd == end;
        
    ASSERT(0);
    return false;
}

IntSize RenderReplaced::intrinsicSize() const
{
    return m_intrinsicSize;
}

void RenderReplaced::setIntrinsicSize(const IntSize& size)
{
    m_intrinsicSize = size;
}

void RenderReplaced::adjustOverflowForBoxShadow()
{
    if (ShadowData* boxShadow = style()->boxShadow()) {
        if (!gOverflowRectMap)
            gOverflowRectMap = new OverflowRectMap();

        IntRect shadow = borderBox();
        shadow.move(boxShadow->x, boxShadow->y);
        shadow.inflate(boxShadow->blur);
        shadow.unite(borderBox());

        gOverflowRectMap->set(this, shadow);
        m_hasOverflow = true;
        return;
    }

    if (m_hasOverflow) {
        gOverflowRectMap->remove(this);
        m_hasOverflow = false;
    }
}

int RenderReplaced::overflowHeight(bool includeInterior) const
{
    if (m_hasOverflow) {
        IntRect *r = &gOverflowRectMap->find(this)->second;
        return r->height() + r->y();
    }

    return height();
}

int RenderReplaced::overflowWidth(bool includeInterior) const
{
    if (m_hasOverflow) {
        IntRect *r = &gOverflowRectMap->find(this)->second;
        return r->width() + r->x();
    }

    return width();
}

int RenderReplaced::overflowLeft(bool includeInterior) const
{
    if (m_hasOverflow)
        return gOverflowRectMap->get(this).x();

    return 0;
}

int RenderReplaced::overflowTop(bool includeInterior) const
{
    if (m_hasOverflow)
        return gOverflowRectMap->get(this).y();

    return 0;
}

IntRect RenderReplaced::overflowRect(bool includeInterior) const
{
    if (m_hasOverflow)
        return gOverflowRectMap->find(this)->second;

    return borderBox();
}

}
