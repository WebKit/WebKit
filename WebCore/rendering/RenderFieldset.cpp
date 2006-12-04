/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderFieldset.h"

#include "HTMLGenericFormElement.h"
#include "HTMLNames.h"

using std::min;
using std::max;

namespace WebCore {

using namespace HTMLNames;

RenderFieldset::RenderFieldset(HTMLGenericFormElement* element)
    : RenderBlock(element)
{
}

RenderObject* RenderFieldset::layoutLegend(bool relayoutChildren)
{
    RenderObject* legend = findLegend();
    if (legend) {
        if (relayoutChildren)
            legend->setNeedsLayout(true);
        legend->layoutIfNeeded();

        int xPos;
        if (style()->direction() == RTL) {
            switch (legend->style()->textAlign()) {
                case LEFT:
                    xPos = borderLeft() + paddingLeft();
                    break;
                case CENTER:
                    xPos = (m_width - legend->width()) / 2;
                    break;
                default:
                    xPos = m_width - paddingRight() - borderRight() - legend->width() - legend->marginRight();
            }
        } else {
            switch (legend->style()->textAlign()) {
                case RIGHT:
                    xPos = m_width - paddingRight() - borderRight() - legend->width();
                    break;
                case CENTER:
                    xPos = (m_width - legend->width()) / 2;
                    break;
                default:
                    xPos = borderLeft() + paddingLeft() + legend->marginLeft();
            }
        }
        int b = borderTop();
        int h = legend->height();
        legend->setPos(xPos, max((b-h)/2, 0));
        m_height = max(b,h) + paddingTop();
    }
    return legend;
}

RenderObject* RenderFieldset::findLegend()
{
    for (RenderObject* legend = firstChild(); legend; legend = legend->nextSibling()) {
        if (!legend->isFloatingOrPositioned() && legend->element() &&
            legend->element()->hasTagName(legendTag))
            return legend;
    }
    return 0;
}

void RenderFieldset::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    int w = width();
    int h = height() + borderTopExtra() + borderBottomExtra();
    RenderObject* legend = findLegend();
    if (!legend)
        return RenderBlock::paintBoxDecorations(paintInfo, tx, ty);

    int yOff = (legend->yPos() > 0) ? 0 : (legend->height() - borderTop()) / 2;
    h -= yOff;
    ty += yOff - borderTopExtra();

    int my = max(ty, paintInfo.rect.y());
    int end = min(paintInfo.rect.bottom(), ty + h);
    int mh = end - my;

    paintBackground(paintInfo.context, style()->backgroundColor(), style()->backgroundLayers(), my, mh, tx, ty, w, h);

    if (style()->hasBorder())
        paintBorderMinusLegend(paintInfo.context, tx, ty, w, h, style(), legend->xPos(), legend->width());
}

void RenderFieldset::paintBorderMinusLegend(GraphicsContext* graphicsContext, int tx, int ty, int w, int h,
                                            const RenderStyle* style, int lx, int lw)
{

    const Color& tc = style->borderTopColor();
    const Color& bc = style->borderBottomColor();

    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool render_t = ts > BHIDDEN;
    bool render_l = ls > BHIDDEN;
    bool render_r = rs > BHIDDEN;
    bool render_b = bs > BHIDDEN;

    if (render_t) {
        drawBorder(graphicsContext, tx, ty, tx + lx, ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE) ? style->borderLeftWidth() : 0), 0);
        drawBorder(graphicsContext, tx + lx + lw, ty, tx + w, ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   0, (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE) ? style->borderRightWidth() : 0));
    }

    if (render_b)
        drawBorder(graphicsContext, tx, ty + h - style->borderBottomWidth(), tx + w, ty + h, BSBottom, bc, style->color(), bs,
                   (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE) ? style->borderLeftWidth() : 0),
                   (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE) ? style->borderRightWidth() : 0));

    if (render_l) {
        const Color& lc = style->borderLeftColor();

        bool ignore_top =
            (tc == lc) &&
            (ls >= OUTSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == lc) &&
            (ls >= OUTSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(graphicsContext, tx, ty, tx + style->borderLeftWidth(), ty + h, BSLeft, lc, style->color(), ls,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());
    }

    if (render_r) {
        const Color& rc = style->borderRightColor();

        bool ignore_top =
            (tc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        drawBorder(graphicsContext, tx + w - style->borderRightWidth(), ty, tx + w, ty + h, BSRight, rc, style->color(), rs,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());
    }
}

void RenderFieldset::setStyle(RenderStyle* newStyle)
{
    RenderBlock::setStyle(newStyle);

    // WinIE renders fieldsets with display:inline like they're inline-blocks.  For us,
    // an inline-block is just a block element with replaced set to true and inline set
    // to true.  Ensure that if we ended up being inline that we set our replaced flag
    // so that we're treated like an inline-block.
    if (isInline())
        setReplaced(true);
}    

} // namespace WebCore
