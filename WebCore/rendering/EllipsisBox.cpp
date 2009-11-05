/**
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
    RenderStyle* style = m_renderer->style(m_firstLine);
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
    context->drawText(style->font(), TextRun(str.characters(), str.length(), false, 0, 0, false, style->visuallyOrdered()), IntPoint(m_x + tx, m_y + ty + style->font().ascent()));

    if (setShadow)
        context->clearShadow();

    if (m_markupBox) {
        // Paint the markup box
        tx += m_x + m_width - m_markupBox->x();
        ty += m_y + style->font().ascent() - (m_markupBox->y() + m_markupBox->renderer()->style(m_firstLine)->font().ascent());
        m_markupBox->paint(paintInfo, tx, ty);
    }
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int x, int y, int tx, int ty)
{
    tx += m_x;
    ty += m_y;

    // Hit test the markup box.
    if (m_markupBox) {
        RenderStyle* style = m_renderer->style(m_firstLine);
        int mtx = tx + m_width - m_markupBox->x();
        int mty = ty + style->font().ascent() - (m_markupBox->y() + m_markupBox->renderer()->style(m_firstLine)->font().ascent());
        if (m_markupBox->nodeAtPoint(request, result, x, y, mtx, mty)) {
            renderer()->updateHitTestResult(result, IntPoint(x - mtx, y - mty));
            return true;
        }
    }

    if (visibleToHitTesting() && IntRect(tx, ty, m_width, m_height).contains(x, y)) {
        renderer()->updateHitTestResult(result, IntPoint(x - tx, y - ty));
        return true;
    }

    return false;
}

} // namespace WebCore
