/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
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
#include "render_br.h"

#include "dom_position.h"
#include "render_block.h"
#include "render_line.h"

using namespace khtml;
using DOM::Position;

RenderBR::RenderBR(DOM::NodeImpl* node)
    : RenderText(node, new DOM::DOMStringImpl(QChar('\n'))), m_x(0), m_y(0), m_height(0),
      m_lineHeight(-1)
{
}

RenderBR::~RenderBR()
{
}

void RenderBR::setPos(int xPos, int yPos)
{
    m_x = xPos;
    m_y = yPos;
}

InlineBox* RenderBR::createInlineBox(bool makePlaceholder, bool isRootLineBox, bool isOnlyRun)
{
    // We only make a box for a <br> if we are on a line by ourself or in strict mode
    // (Note the use of strict mode.  In "almost strict" mode, we don't make a box for <br>.)
    if (isOnlyRun || document()->inStrictMode())
        return RenderText::createInlineBox(makePlaceholder, isRootLineBox, isOnlyRun);
    return 0;
}

void RenderBR::position(InlineBox* box, int from, int len, bool reverse)
{
    InlineTextBox *s = static_cast<InlineTextBox*>(box);
    
    // We want the box to be destroyed, but get the position of it first.
    m_x = s->xPos();
    m_y = s->yPos();
    m_height = s->height();
}

short RenderBR::lineHeight(bool firstLine, bool isRootLineBox) const
{
    if (firstLine) {
        RenderStyle* s = style(firstLine);
        Length lh = s->lineHeight();
        if (lh.value < 0) {
            if (s == style()) {
                if (m_lineHeight == -1)
                    m_lineHeight = RenderObject::lineHeight(false);
                return m_lineHeight;
            }
            return s->fontMetrics().lineSpacing();
	}
        if (lh.isPercent())
            return lh.minWidth(s->font().pixelSize());
        return lh.value;
    }
    
    if (m_lineHeight == -1)
        m_lineHeight = RenderObject::lineHeight(false);
    return m_lineHeight;
}

void RenderBR::setStyle(RenderStyle* _style)
{
    RenderText::setStyle(_style);
    m_lineHeight = -1;
}

long RenderBR::caretMinOffset() const 
{ 
    return 0; 
}

long RenderBR::caretMaxOffset() const 
{ 
    return 1; 
}

unsigned long RenderBR::caretMaxRenderedOffset() const
{
    return 1;
}

Position RenderBR::positionForCoordinates(int _x, int _y)
{
    return Position(element(), 0);
}

QRect RenderBR::caretRect(int offset, bool override)
{
    // EDIT FIXME: This does not work yet. Some other changes are need before
    // an accurate position can be determined.

    int absx, absy;
    absolutePosition(absx,absy);

    // FIXME: an older version of this code wasn't setting width at
    // all, using the default of 1...
    return QRect(xPos() + absx, yPos() + absy, 1, lineHeight(false));
}

InlineBox *RenderBR::inlineBox(long offset)
{
    return firstTextBox();
}

// FIXME: This is temporary until we move line extension painting into the block.
QRect RenderBR::selectionRect()
{
    if (!firstTextBox() || selectionState() == SelectionNone)
        return QRect(0,0,0,0);
    
    RenderBlock *cb = containingBlock();
    RootInlineBox* root = firstTextBox()->root();
    int selectionTop = root->prevRootBox() ? root->prevRootBox()->bottomOverflow() : root->topOverflow();
    int selectionHeight = root->bottomOverflow() - selectionTop;
    int selectionLeft = xPos();
    RenderObject *prevLineLastLeaf = (root->prevRootBox() && root->prevRootBox()->lastLeafChild()) ? root->prevRootBox()->lastLeafChild()->object() : 0;
    if (root->firstLeafChild() == firstTextBox() && root->prevRootBox() && prevLineLastLeaf && 
        prevLineLastLeaf->selectionState() != RenderObject::SelectionNone)
        selectionLeft = kMax(cb->leftOffset(selectionTop), cb->leftOffset(root->blockHeight()));
    
    // Extending to the end of the line is "automatic" with BR's.
    int selectionRight = kMin(cb->rightOffset(selectionTop), cb->rightOffset(root->blockHeight()));
    int selectionWidth = selectionRight - selectionLeft;
    
    int absx, absy;
    cb->absolutePosition(absx, absy);
    return QRect(selectionLeft + absx, selectionTop + absy, selectionWidth, selectionHeight);
}

// FIXME: This is just temporary until line extension painting moves into the block.
void RenderBR::paint(PaintInfo& i, int tx, int ty)
{
#if APPLE_CHANGES
    if (!firstTextBox() || selectionState() == SelectionNone || (i.phase != PaintActionForeground && i.phase != PaintActionSelection))
        return;

    bool isPrinting = (i.p->device()->devType() == QInternal::Printer);
    if (isPrinting)
        return;
    
    // Do the calculations to draw selections as tall as the line.
    // Use the bottom of the line above as the y position (if there is one, 
    // otherwise use the top of this renderer's line) and the height of the line as the height. 
    // This mimics Cocoa.
    int y = 0;
    RootInlineBox *root = firstTextBox()->root();
    if (root->prevRootBox())
        y = root->prevRootBox()->bottomOverflow();
    else
        y = root->topOverflow();

    int h = root->bottomOverflow() - y;

    RenderBlock *cb = containingBlock();
 
    // Extend selection to the start of the line if:
    // 1. The starting point of the selection is at or beyond the start of the text box; and
    // 2. This box is the first box on the line; and
    // 3. There is a another line before this one (first lines have special behavior;
    //    the selection never extends on the first line); and 
    // 4. The last leaf renderer on the previous line is selected.
    int x = firstTextBox()->xPos();
    RenderObject *prevLineLastLeaf = root->prevRootBox() ? root->prevRootBox()->lastLeafChild()->object() : 0;
    if (root->firstLeafChild() == firstTextBox() && root->prevRootBox() && prevLineLastLeaf && 
        prevLineLastLeaf->selectionState() != RenderObject::SelectionNone)
        x = kMax(cb->leftOffset(y), cb->leftOffset(root->blockHeight()));

    // Extending to the end of the line is "automatic" with BR's.
    int maxX = kMin(cb->rightOffset(y), cb->rightOffset(root->blockHeight()));
    int w = maxX - x;
    
    // Macintosh-style text highlighting is to draw with a particular background color, not invert.
    i.p->save();
    QColor textColor = style()->color();
    QColor c = i.p->selectedTextBackgroundColor();
    
    // if text color and selection background color are identical, invert background color.
    if (textColor == c)
        c = QColor(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    RenderStyle* pseudoStyle = getPseudoStyle(RenderStyle::SELECTION);
    if (pseudoStyle && pseudoStyle->backgroundColor().isValid())
        c = pseudoStyle->backgroundColor();
    
    QBrush brush = i.p->brush();
    brush.setColor(c);
    i.p->fillRect(x + tx, y + ty, w, h, brush);
    i.p->restore();
#endif
}
