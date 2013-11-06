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
#include "Font.h"
#include "GraphicsContext.h"
#include "HitTestResult.h"
#include "InlineTextBox.h"
#include "PaintInfo.h"
#include "RootInlineBox.h"
#include "TextRun.h"

namespace WebCore {

EllipsisBox::EllipsisBox(RenderBlockFlow& renderer, const AtomicString& ellipsisStr, InlineFlowBox* parent, int width, int height, int y, bool firstLine, bool isVertical, InlineBox* markupBox)
    : InlineElementBox(renderer, FloatPoint(0, y), width, firstLine, true, false, false, isVertical, 0, 0, parent)
    , m_shouldPaintMarkupBox(markupBox)
    , m_height(height)
    , m_str(ellipsisStr)
    , m_selectionState(RenderObject::SelectionNone)
{
}

void EllipsisBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    GraphicsContext* context = paintInfo.context;
    const RenderStyle& lineStyle = this->lineStyle();
    Color textColor = lineStyle.visitedDependentColor(CSSPropertyWebkitTextFillColor);
    if (textColor != context->fillColor())
        context->setFillColor(textColor, lineStyle.colorSpace());
    bool setShadow = false;
    if (lineStyle.textShadow()) {
        context->setShadow(LayoutSize(lineStyle.textShadow()->x(), lineStyle.textShadow()->y()),
            lineStyle.textShadow()->radius(), lineStyle.textShadow()->color(), lineStyle.colorSpace());
        setShadow = true;
    }

    const Font& font = lineStyle.font();
    if (selectionState() != RenderObject::SelectionNone) {
        paintSelection(context, paintOffset, lineStyle, font);

        // Select the correct color for painting the text.
        Color foreground = paintInfo.forceBlackText() ? Color::black : blockFlow().selectionForegroundColor();
        if (foreground.isValid() && foreground != textColor)
            context->setFillColor(foreground, lineStyle.colorSpace());
    }

    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    context->drawText(font, RenderBlock::constructTextRun(&blockFlow(), font, m_str, lineStyle, TextRun::AllowTrailingExpansion), LayoutPoint(x() + paintOffset.x(), y() + paintOffset.y() + lineStyle.fontMetrics().ascent()));

    // Restore the regular fill color.
    if (textColor != context->fillColor())
        context->setFillColor(textColor, lineStyle.colorSpace());

    if (setShadow)
        context->clearShadow();

    paintMarkupBox(paintInfo, paintOffset, lineTop, lineBottom, lineStyle);
}

InlineBox* EllipsisBox::markupBox() const
{
    if (!m_shouldPaintMarkupBox)
        return 0;

    RootInlineBox* lastLine = blockFlow().lineAtIndex(blockFlow().lineCount() - 1);
    if (!lastLine)
        return 0;

    // If the last line-box on the last line of a block is a link, -webkit-line-clamp paints that box after the ellipsis.
    // It does not actually move the link.
    InlineBox* anchorBox = lastLine->lastChild();
    if (!anchorBox || !anchorBox->renderer().style().isLink())
        return 0;

    return anchorBox;
}

void EllipsisBox::paintMarkupBox(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const RenderStyle& style)
{
    InlineBox* markupBox = this->markupBox();
    if (!markupBox)
        return;

    LayoutPoint adjustedPaintOffset = paintOffset;
    adjustedPaintOffset.move(x() + m_logicalWidth - markupBox->x(),
        y() + style.fontMetrics().ascent() - (markupBox->y() + markupBox->lineStyle().fontMetrics().ascent()));
    markupBox->paint(paintInfo, adjustedPaintOffset, lineTop, lineBottom);
}

IntRect EllipsisBox::selectionRect()
{
    const RenderStyle& lineStyle = this->lineStyle();
    const Font& font = lineStyle.font();
    const RootInlineBox& rootBox = root();
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    return enclosingIntRect(font.selectionRectForText(RenderBlock::constructTextRun(&blockFlow(), font, m_str, lineStyle, TextRun::AllowTrailingExpansion), IntPoint(x(), y() + rootBox.selectionTopAdjustedForPrecedingBlock()), rootBox.selectionHeightAdjustedForPrecedingBlock()));
}

void EllipsisBox::paintSelection(GraphicsContext* context, const LayoutPoint& paintOffset, const RenderStyle& style, const Font& font)
{
    Color textColor = style.visitedDependentColor(CSSPropertyColor);
    Color c = blockFlow().selectionBackgroundColor();
    if (!c.isValid() || !c.alpha())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    const RootInlineBox& rootBox = root();
    LayoutUnit top = rootBox.selectionTop();
    LayoutUnit h = rootBox.selectionHeight();
    FloatRect clipRect(x() + paintOffset.x(), top + paintOffset.y(), m_logicalWidth, h);
    alignSelectionRectToDevicePixels(clipRect);

    GraphicsContextStateSaver stateSaver(*context);
    context->clip(clipRect);
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    context->drawHighlightForText(font, RenderBlock::constructTextRun(&blockFlow(), font, m_str, style, TextRun::AllowTrailingExpansion), roundedIntPoint(LayoutPoint(x() + paintOffset.x(), y() + paintOffset.y() + top)), h, c, style.colorSpace());
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const HitTestLocation& locationInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutPoint adjustedLocation = accumulatedOffset + roundedLayoutPoint(topLeft());

    // Hit test the markup box.
    if (InlineBox* markupBox = this->markupBox()) {
        const RenderStyle& lineStyle = this->lineStyle();
        LayoutUnit mtx = adjustedLocation.x() + m_logicalWidth - markupBox->x();
        LayoutUnit mty = adjustedLocation.y() + lineStyle.fontMetrics().ascent() - (markupBox->y() + markupBox->lineStyle().fontMetrics().ascent());
        if (markupBox->nodeAtPoint(request, result, locationInContainer, LayoutPoint(mtx, mty), lineTop, lineBottom)) {
            blockFlow().updateHitTestResult(result, locationInContainer.point() - LayoutSize(mtx, mty));
            return true;
        }
    }

    LayoutRect boundsRect(adjustedLocation, LayoutSize(m_logicalWidth, m_height));
    if (visibleToHitTesting() && boundsRect.intersects(HitTestLocation::rectForPoint(locationInContainer.point(), 0, 0, 0, 0))) {
        blockFlow().updateHitTestResult(result, locationInContainer.point() - toLayoutSize(adjustedLocation));
        if (!result.addNodeToRectBasedTestResult(blockFlow().element(), request, locationInContainer, boundsRect))
            return true;
    }

    return false;
}

} // namespace WebCore
