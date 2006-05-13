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
// -------------------------------------------------------------------------

#include "config.h"
#include "EllipsisBox.h"

#include "GraphicsContext.h"

namespace WebCore {

void EllipsisBox::paint(RenderObject::PaintInfo& i, int _tx, int _ty)
{
    GraphicsContext* p = i.p;
    RenderStyle* _style = m_firstLine ? m_object->firstLineStyle() : m_object->style();
    if (_style->font() != p->font())
        p->setFont(_style->font());

    Color textColor = _style->color();
    if (textColor != p->pen().color())
        p->setPen(textColor);
    bool setShadow = false;
    if (_style->textShadow()) {
        p->setShadow(IntSize(_style->textShadow()->x, _style->textShadow()->y),
                     _style->textShadow()->blur, _style->textShadow()->color);
        setShadow = true;
    }

    const String& str = m_str;
    p->drawText(TextRun(str.impl()), IntPoint(m_x + _tx, m_y + _ty + m_baseline), TextStyle(0, 0, 0, false, _style->visuallyOrdered()));

    if (setShadow)
        p->clearShadow();

    if (m_markupBox) {
        // Paint the markup box
        _tx += m_x + m_width - m_markupBox->xPos();
        _ty += m_y + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        m_markupBox->paint(i, _tx, _ty);
    }
}

bool EllipsisBox::nodeAtPoint(RenderObject::NodeInfo& info, int x, int y, int tx, int ty)
{
    tx += m_x;
    ty += m_y;

    // Hit test the markup box.
    if (m_markupBox) {
        int mtx = tx + m_width - m_markupBox->xPos();
        int mty = ty + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        if (m_markupBox->nodeAtPoint(info, x, y, mtx, mty)) {
            object()->setInnerNode(info);
            return true;
        }
    }

    if (object()->style()->visibility() == VISIBLE && IntRect(tx, ty, m_width, m_height).contains(x, y)) {
        object()->setInnerNode(info);
        return true;
    }

    return false;
}

}
