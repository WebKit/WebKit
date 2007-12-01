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
#include "EllipsisBox.h"

#include "Document.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"

namespace WebCore {

void EllipsisBox::paint(RenderObject::PaintInfo& paintInfo, int tx, int ty)
{
    GraphicsContext* context = paintInfo.context;
    RenderStyle* style = m_object->style(m_firstLine);
    if (style->font() != context->font())
        context->setFont(style->font());

    Color textColor = style->color();
    if (textColor != context->fillColor())
        context->setFillColor(textColor);
    bool setShadow = false;
    if (style->textShadow()) {
        context->setShadow(IntSize(style->textShadow()->x, style->textShadow()->y),
                           style->textShadow()->blur, style->textShadow()->color);
        setShadow = true;
    }

    const String& str = m_str;
    context->drawText(TextRun(str.characters(), str.length(), false, 0, 0, false, style->visuallyOrdered()), IntPoint(m_x + tx, m_y + ty + m_baseline));

    if (setShadow)
        context->clearShadow();

    if (m_markupBox) {
        // Paint the markup box
        tx += m_x + m_width - m_markupBox->xPos();
        ty += m_y + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        m_markupBox->paint(paintInfo, tx, ty);
    }
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    tx += m_x;
    ty += m_y;

    // Hit test the markup box.
    if (m_markupBox) {
        int mtx = tx + m_width - m_markupBox->xPos();
        int mty = ty + m_baseline - (m_markupBox->yPos() + m_markupBox->baseline());
        if (m_markupBox->nodeAtPoint(request, result, x, y, mtx, mty)) {
            object()->updateHitTestResult(result, IntPoint(x - mtx, y - mty));
            return true;
        }
    }

    if (object()->style()->visibility() == VISIBLE && IntRect(tx, ty, m_width, m_height).contains(x, y)) {
        object()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }

    return false;
}

} // namespace WebCore
