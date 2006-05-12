/**
 * This file is part of the HTML widget for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006 Apple Computer, Inc.
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
 *
 */
#include "config.h"
#include "RenderReplaced.h"

#include "render_line.h"
#include "RenderBlock.h"

using namespace std;

namespace WebCore {

RenderReplaced::RenderReplaced(Node* node)
    : RenderBox(node)
{
    // init RenderObject attributes
    setReplaced(true);

    m_intrinsicWidth = 300;
    m_intrinsicHeight = 150;
    m_selectionState = SelectionNone;
}

bool RenderReplaced::shouldPaint(PaintInfo& i, int& _tx, int& _ty)
{
    if (i.phase != PaintPhaseForeground && i.phase != PaintPhaseOutline && i.phase != PaintPhaseSelfOutline 
        && i.phase != PaintPhaseSelection)
        return false;

    if (!shouldPaintWithinRoot(i))
        return false;
        
    // if we're invisible or haven't received a layout yet, then just bail.
    if (style()->visibility() != VISIBLE || m_y <=  -500000)  return false;

    int tx = _tx + m_x;
    int ty = _ty + m_y;

    // Early exit if the element touches the edges.
    int top = ty;
    int bottom = ty + m_height;
    if (isSelected() && m_inlineBoxWrapper) {
        int selTop = _ty + m_inlineBoxWrapper->root()->selectionTop();
        int selBottom = _ty + selTop + m_inlineBoxWrapper->root()->selectionHeight();
        top = min(selTop, top);
        bottom = max(selBottom, bottom);
    }
    
    int os = 2*maximalOutlineSize(i.phase);
    if (tx >= i.r.right() + os || tx + m_width <= i.r.x() - os)
        return false;
    if (top >= i.r.bottom() + os || bottom <= i.r.y() - os)
        return false;

    return true;
}

void RenderReplaced::calcMinMaxWidth()
{
    KHTMLAssert( !minMaxKnown());

#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << "RenderReplaced::calcMinMaxWidth() known=" << minMaxKnown() << endl;
#endif

    int width = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();
    if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent())) {
        m_minWidth = 0;
        m_maxWidth = width;
    } else
        m_minWidth = m_maxWidth = width;

    setMinMaxKnown();
}

short RenderReplaced::lineHeight( bool, bool ) const
{
    return height()+marginTop()+marginBottom();
}

short RenderReplaced::baselinePosition( bool, bool ) const
{
    return height()+marginTop()+marginBottom();
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

VisiblePosition RenderReplaced::positionForCoordinates(int _x, int _y)
{
    InlineBox *box = inlineBoxWrapper();
    if (!box)
        return VisiblePosition(element(), 0, DOWNSTREAM);

    RootInlineBox *root = box->root();

    int absx, absy;
    containingBlock()->absolutePosition(absx, absy);

    int top = absy + root->topOverflow();
    int bottom = root->nextRootBox() ? absy + root->nextRootBox()->topOverflow() : absy + root->bottomOverflow();

    if (_y < top)
        return VisiblePosition(element(), caretMinOffset(), DOWNSTREAM); // coordinates are above
    
    if (_y >= bottom)
        return VisiblePosition(element(), caretMaxOffset(), DOWNSTREAM); // coordinates are below
    
    if (element()) {
        if (_x <= absx + xPos() + (width() / 2))
            return VisiblePosition(element(), 0, DOWNSTREAM);

        return VisiblePosition(element(), 1, DOWNSTREAM);
    }

    return RenderBox::positionForCoordinates(_x, _y);
}

IntRect RenderReplaced::selectionRect()
{
    if (!isSelected())
        return IntRect();
    if (!m_inlineBoxWrapper)
        // We're a block-level replaced element.  Just return our own dimensions.
        return absoluteBoundingBoxRect();

    RenderBlock* cb =  containingBlock();
    if (!cb)
        return IntRect();
    
    RootInlineBox* root = m_inlineBoxWrapper->root();
    int selectionTop = root->selectionTop();
    int selectionHeight = root->selectionHeight();
    int selectionLeft = xPos();
    int selectionRight = xPos() + width();
    
    int absx, absy;
    cb->absolutePosition(absx, absy);
    if (cb->hasOverflowClip())
        cb->layer()->subtractScrollOffset(absx, absy);

    return IntRect(selectionLeft + absx, selectionTop + absy, selectionRight - selectionLeft, selectionHeight);
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

bool RenderReplaced::isSelected()
{
    SelectionState s = selectionState();
    if (s == SelectionNone)
        return false;
    if (s == SelectionInside)
        return true;

    int selectionStart, selectionEnd;
    RenderObject::selectionStartEnd(selectionStart, selectionEnd);
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

Color RenderReplaced::selectionColor(GraphicsContext* p) const
{
    Color color = RenderBox::selectionColor(p);
         
    // Limit the opacity so that no user-specified selection color can obscure selected images.
    if (color.alpha() > selectionColorImageOverlayAlpha)
        color = Color(color.red(), color.green(), color.blue(), selectionColorImageOverlayAlpha);

    return color;
}
}
