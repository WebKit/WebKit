/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
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

#include "CSSPropertyNames.h"
#include "GraphicsContext.h"
#include "HTMLFieldSetElement.h"
#include "HTMLNames.h"
#include "PaintInfo.h"

namespace WebCore {

using namespace HTMLNames;

RenderFieldset::RenderFieldset(HTMLFieldSetElement& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, WTF::move(style))
{
}

void RenderFieldset::computePreferredLogicalWidths()
{
    RenderBlockFlow::computePreferredLogicalWidths();
    if (RenderBox* legend = findLegend()) {
        int legendMinWidth = legend->minPreferredLogicalWidth();

        Length legendMarginLeft = legend->style().marginLeft();
        Length legendMarginRight = legend->style().marginLeft();

        if (legendMarginLeft.isFixed())
            legendMinWidth += legendMarginLeft.value();

        if (legendMarginRight.isFixed())
            legendMinWidth += legendMarginRight.value();

        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, legendMinWidth + horizontalBorderAndPaddingExtent());
    }
}

RenderObject* RenderFieldset::layoutSpecialExcludedChild(bool relayoutChildren)
{
    RenderBox* box = findLegend();
    if (!box)
        return nullptr;

    RenderBox& legend = *box;
    if (relayoutChildren)
        legend.setNeedsLayout();
    legend.layoutIfNeeded();

    LayoutUnit logicalLeft;
    if (style().isLeftToRightDirection()) {
        switch (legend.style().textAlign()) {
        case CENTER:
            logicalLeft = (logicalWidth() - logicalWidthForChild(legend)) / 2;
            break;
        case RIGHT:
            logicalLeft = logicalWidth() - borderEnd() - paddingEnd() - logicalWidthForChild(legend);
            break;
        default:
            logicalLeft = borderStart() + paddingStart() + marginStartForChild(legend);
            break;
        }
    } else {
        switch (legend.style().textAlign()) {
        case LEFT:
            logicalLeft = borderStart() + paddingStart();
            break;
        case CENTER: {
            // Make sure that the extra pixel goes to the end side in RTL (since it went to the end side
            // in LTR).
            LayoutUnit centeredWidth = logicalWidth() - logicalWidthForChild(legend);
            logicalLeft = centeredWidth - centeredWidth / 2;
            break;
        }
        default:
            logicalLeft = logicalWidth() - borderStart() - paddingStart() - marginStartForChild(legend) - logicalWidthForChild(legend);
            break;
        }
    }

    setLogicalLeftForChild(legend, logicalLeft);

    LayoutUnit fieldsetBorderBefore = borderBefore();
    LayoutUnit legendLogicalHeight = logicalHeightForChild(legend);

    LayoutUnit legendLogicalTop;
    LayoutUnit collapsedLegendExtent;
    // FIXME: We need to account for the legend's margin before too.
    if (fieldsetBorderBefore > legendLogicalHeight) {
        // The <legend> is smaller than the associated fieldset before border
        // so the latter determines positioning of the <legend>. The sizing depends
        // on the legend's margins as we want to still follow the author's cues.
        // Firefox completely ignores the margins in this case which seems wrong.
        legendLogicalTop = (fieldsetBorderBefore - legendLogicalHeight) / 2;
        collapsedLegendExtent = std::max<LayoutUnit>(fieldsetBorderBefore, legendLogicalTop + legendLogicalHeight + marginAfterForChild(legend));
    } else
        collapsedLegendExtent = legendLogicalHeight + marginAfterForChild(legend);

    setLogicalTopForChild(legend, legendLogicalTop);
    setLogicalHeight(paddingBefore() + collapsedLegendExtent);

    return &legend;
}

RenderBox* RenderFieldset::findLegend(FindLegendOption option) const
{
    for (RenderObject* legend = firstChild(); legend; legend = legend->nextSibling()) {
        if (option == IgnoreFloatingOrOutOfFlow && legend->isFloatingOrOutOfFlowPositioned())
            continue;
        
        if (legend->node() && (legend->node()->hasTagName(legendTag)))
            return toRenderBox(legend);
    }
    return nullptr;
}

void RenderFieldset::paintBoxDecorations(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!paintInfo.shouldPaintWithinRoot(*this))
        return;

    LayoutRect paintRect(paintOffset, size());
    RenderBox* legend = findLegend();
    if (!legend)
        return RenderBlockFlow::paintBoxDecorations(paintInfo, paintOffset);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    if (style().isHorizontalWritingMode()) {
        LayoutUnit yOff = (legend->y() > 0) ? LayoutUnit() : (legend->height() - borderTop()) / 2;
        paintRect.setHeight(paintRect.height() - yOff);
        paintRect.setY(paintRect.y() + yOff);
    } else {
        LayoutUnit xOff = (legend->x() > 0) ? LayoutUnit() : (legend->width() - borderLeft()) / 2;
        paintRect.setWidth(paintRect.width() - xOff);
        paintRect.setX(paintRect.x() + xOff);
    }

    if (!boxShadowShouldBeAppliedToBackground(determineBackgroundBleedAvoidance(paintInfo.context)))
        paintBoxShadow(paintInfo, paintRect, style(), Normal);
    paintFillLayers(paintInfo, style().visitedDependentColor(CSSPropertyBackgroundColor), style().backgroundLayers(), paintRect);
    paintBoxShadow(paintInfo, paintRect, style(), Inset);

    if (!style().hasBorder())
        return;
    
    // Create a clipping region around the legend and paint the border as normal
    GraphicsContext* graphicsContext = paintInfo.context;
    GraphicsContextStateSaver stateSaver(*graphicsContext);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    LayoutRect clipRect;
    if (style().isHorizontalWritingMode()) {
        clipRect.setX(paintRect.x() + legend->x());
        clipRect.setY(paintRect.y());
        clipRect.setWidth(legend->width());
        clipRect.setHeight(std::max<LayoutUnit>(style().borderTopWidth(), legend->height() - ((legend->height() - borderTop()) / 2)));
    } else {
        clipRect.setX(paintRect.x());
        clipRect.setY(paintRect.y() + legend->y());
        clipRect.setWidth(std::max<LayoutUnit>(style().borderLeftWidth(), legend->width()));
        clipRect.setHeight(legend->height());
    }
    graphicsContext->clipOut(pixelSnappedForPainting(clipRect, document().deviceScaleFactor()));

    paintBorder(paintInfo, paintRect, style());
}

void RenderFieldset::paintMask(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != VISIBLE || paintInfo.phase != PaintPhaseMask)
        return;

    LayoutRect paintRect = LayoutRect(paintOffset, size());
    RenderBox* legend = findLegend();
    if (!legend)
        return RenderBlockFlow::paintMask(paintInfo, paintOffset);

    // FIXME: We need to work with "rl" and "bt" block flow directions.  In those
    // cases the legend is embedded in the right and bottom borders respectively.
    // https://bugs.webkit.org/show_bug.cgi?id=47236
    if (style().isHorizontalWritingMode()) {
        LayoutUnit yOff = (legend->y() > 0) ? LayoutUnit() : (legend->height() - borderTop()) / 2;
        paintRect.expand(0, -yOff);
        paintRect.move(0, yOff);
    } else {
        LayoutUnit xOff = (legend->x() > 0) ? LayoutUnit() : (legend->width() - borderLeft()) / 2;
        paintRect.expand(-xOff, 0);
        paintRect.move(xOff, 0);
    }

    paintMaskImages(paintInfo, paintRect);
}

} // namespace WebCore
