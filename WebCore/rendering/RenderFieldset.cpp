/*
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "RenderFieldset.h"

#include "HTMLNames.h"
#include "GraphicsContext.h"

#if ENABLE(WML)
#include "WMLNames.h"
#endif

using std::min;
using std::max;

namespace WebCore {

using namespace HTMLNames;

RenderFieldset::RenderFieldset(Node* element)
    : RenderBlock(element)
{
}

void RenderFieldset::calcPrefWidths()
{
    RenderBlock::calcPrefWidths();
    if (RenderBox* legend = findLegend()) {
        int legendMinWidth = legend->minPrefWidth();

        Length legendMarginLeft = legend->style()->marginLeft();
        Length legendMarginRight = legend->style()->marginLeft();

        if (legendMarginLeft.isFixed())
            legendMinWidth += legendMarginLeft.value();

        if (legendMarginRight.isFixed())
            legendMinWidth += legendMarginRight.value();

        m_minPrefWidth = max(m_minPrefWidth, legendMinWidth + paddingLeft() + paddingRight() + borderLeft() + borderRight());
    }
}

RenderObject* RenderFieldset::layoutLegend(bool relayoutChildren)
{
    RenderBox* legend = findLegend();
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
                    xPos = (width() - legend->width()) / 2;
                    break;
                default:
                    xPos = width() - paddingRight() - borderRight() - legend->width() - legend->marginRight();
            }
        } else {
            switch (legend->style()->textAlign()) {
                case RIGHT:
                    xPos = width() - paddingRight() - borderRight() - legend->width();
                    break;
                case CENTER:
                    xPos = (width() - legend->width()) / 2;
                    break;
                default:
                    xPos = borderLeft() + paddingLeft() + legend->marginLeft();
            }
        }
        int b = borderTop();
        int h = legend->height();
        legend->setLocation(xPos, max((b-h)/2, 0));
        setHeight(max(b, h) + paddingTop());
    }
    return legend;
}

RenderBox* RenderFieldset::findLegend() const
{
    for (RenderObject* legend = firstChild(); legend; legend = legend->nextSibling()) {
        if (!legend->isFloatingOrPositioned() && legend->node() &&
            (legend->node()->hasTagName(legendTag)
#if ENABLE(WML)
            || legend->node()->hasTagName(WMLNames::insertedLegendTag)
#endif
            )
           )
            return toRenderBox(legend);
    }
    return 0;
}

void RenderFieldset::paintBoxDecorations(PaintInfo& paintInfo, int tx, int ty)
{
    if (!shouldPaintWithinRoot(paintInfo))
        return;

    int w = width();
    int h = height();
    RenderBox* legend = findLegend();
    if (!legend)
        return RenderBlock::paintBoxDecorations(paintInfo, tx, ty);

    int yOff = (legend->y() > 0) ? 0 : (legend->height() - borderTop()) / 2;
    int legendBottom = ty + legend->y() + legend->height();
    h -= yOff;
    ty += yOff;

    paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Normal);

    paintFillLayers(paintInfo, style()->backgroundColor(), style()->backgroundLayers(), tx, ty, w, h);
    paintBoxShadow(paintInfo.context, tx, ty, w, h, style(), Inset);

    if (!style()->hasBorder())
        return;

    // Save time by not saving and restoring the GraphicsContext in the straight border case
    if (!style()->hasBorderRadius())
        return paintBorderMinusLegend(paintInfo.context, tx, ty, w, h, style(), legend->x(), legend->width(), legendBottom);
    
    // We have rounded borders, create a clipping region 
    // around the legend and paint the border as normal
    GraphicsContext* graphicsContext = paintInfo.context;
    graphicsContext->save();

    int clipTop = ty;
    int clipHeight = max(static_cast<int>(style()->borderTopWidth()), legend->height());

    graphicsContext->clipOut(IntRect(tx + legend->x(), clipTop,
                                       legend->width(), clipHeight));
    paintBorder(paintInfo.context, tx, ty, w, h, style(), true, true);

    graphicsContext->restore();
}

void RenderFieldset::paintMask(PaintInfo& paintInfo, int tx, int ty)
{
    if (style()->visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    int w = width();
    int h = height();
    RenderBox* legend = findLegend();
    if (!legend)
        return RenderBlock::paintMask(paintInfo, tx, ty);

    int yOff = (legend->y() > 0) ? 0 : (legend->height() - borderTop()) / 2;
    h -= yOff;
    ty += yOff;

    paintMaskImages(paintInfo, tx, ty, w, h);
}
        
void RenderFieldset::paintBorderMinusLegend(GraphicsContext* graphicsContext, int tx, int ty, int w, int h,
                                            const RenderStyle* style, int lx, int lw, int lb)
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

    int borderLeftWidth = style->borderLeftWidth();
    int borderRightWidth = style->borderRightWidth();

    if (render_t) {
        if (lx >= borderLeftWidth)
            drawLineForBoxSide(graphicsContext, tx, ty, tx + min(lx, w), ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                       (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE) ? borderLeftWidth : 0),
                       (lx >= w && render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE) ? borderRightWidth : 0));
        if (lx + lw <=  w - borderRightWidth)
            drawLineForBoxSide(graphicsContext, tx + max(0, lx + lw), ty, tx + w, ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                       (lx + lw <= 0 && render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE) ? borderLeftWidth : 0),
                       (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE) ? borderRightWidth : 0));
    }

    if (render_b)
        drawLineForBoxSide(graphicsContext, tx, ty + h - style->borderBottomWidth(), tx + w, ty + h, BSBottom, bc, style->color(), bs,
                   (render_l && (ls == DOTTED || ls == DASHED || ls == DOUBLE) ? style->borderLeftWidth() : 0),
                   (render_r && (rs == DOTTED || rs == DASHED || rs == DOUBLE) ? style->borderRightWidth() : 0));

    if (render_l) {
        const Color& lc = style->borderLeftColor();
        int startY = ty;

        bool ignore_top =
            (tc == lc) &&
            (ls >= OUTSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == lc) &&
            (ls >= OUTSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        if (lx < borderLeftWidth && lx + lw > 0) {
            // The legend intersects the border.
            ignore_top = true;
            startY = lb;
        }

        drawLineForBoxSide(graphicsContext, tx, startY, tx + borderLeftWidth, ty + h, BSLeft, lc, style->color(), ls,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());
    }

    if (render_r) {
        const Color& rc = style->borderRightColor();
        int startY = ty;

        bool ignore_top =
            (tc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET);

        bool ignore_bottom =
            (bc == rc) &&
            (rs >= DOTTED || rs == INSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET);

        if (lx < w && lx + lw > w - borderRightWidth) {
            // The legend intersects the border.
            ignore_top = true;
            startY = lb;
        }

        drawLineForBoxSide(graphicsContext, tx + w - borderRightWidth, startY, tx + w, ty + h, BSRight, rc, style->color(), rs,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());
    }
}

void RenderFieldset::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    // WinIE renders fieldsets with display:inline like they're inline-blocks.  For us,
    // an inline-block is just a block element with replaced set to true and inline set
    // to true.  Ensure that if we ended up being inline that we set our replaced flag
    // so that we're treated like an inline-block.
    if (isInline())
        setReplaced(true);
}    

} // namespace WebCore
