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
#include "xml/dom_position.h"

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

void RenderBR::caretPos(int offset, bool override, int &_x, int &_y, int &_w, int &_h)
{
    // EDIT FIXME: This does not work yet. Some other changes are need before
    // an accurate position can be determined.
    _h = lineHeight(false);
    _x = xPos();
    _y = yPos();

    int absx, absy;
    absolutePosition(absx,absy);
    _x += absx;
    _y += absy;
}

InlineBox *RenderBR::inlineBox(long offset)
{
    return firstTextBox();
}
