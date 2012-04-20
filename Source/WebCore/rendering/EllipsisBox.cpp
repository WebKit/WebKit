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
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RootInlineBox.h"
#include "TextRun.h"

namespace WebCore {

void EllipsisBox::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    GraphicsContext* context = paintInfo.context;
    RenderStyle* style = m_renderer->style(isFirstLineStyle());
    Color textColor = style->visitedDependentColor(CSSPropertyColor);
    if (textColor != context->fillColor())
        context->setFillColor(textColor, style->colorSpace());
    bool setShadow = false;
    if (style->textShadow()) {
        context->setShadow(LayoutSize(style->textShadow()->x(), style->textShadow()->y()),
                           style->textShadow()->blur(), style->textShadow()->color(), style->colorSpace());
        setShadow = true;
    }

    const Font& font = style->font();
    if (selectionState() != RenderObject::SelectionNone) {
        paintSelection(context, paintOffset, style, font);

        // Select the correct color for painting the text.
        Color foreground = paintInfo.forceBlackText ? Color::black : renderer()->selectionForegroundColor();
        if (foreground.isValid() && foreground != textColor)
            context->setFillColor(foreground, style->colorSpace());
    }

    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    context->drawText(font, RenderBlock::constructTextRun(renderer(), font, m_str, style, TextRun::AllowTrailingExpansion), LayoutPoint(x() + paintOffset.x(), y() + paintOffset.y() + style->fontMetrics().ascent()));

    // Restore the regular fill color.
    if (textColor != context->fillColor())
        context->setFillColor(textColor, style->colorSpace());

    if (setShadow)
        context->clearShadow();

    if (m_markupBox) {
        // Paint the markup box
        LayoutPoint adjustedPaintOffset = paintOffset;
        adjustedPaintOffset.move(x() + m_logicalWidth - m_markupBox->x(),
            y() + style->fontMetrics().ascent() - (m_markupBox->y() + m_markupBox->renderer()->style(isFirstLineStyle())->fontMetrics().ascent()));
        m_markupBox->paint(paintInfo, adjustedPaintOffset, lineTop, lineBottom);
    }
}

IntRect EllipsisBox::selectionRect()
{
    RenderStyle* style = m_renderer->style(isFirstLineStyle());
    const Font& font = style->font();
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    return enclosingIntRect(font.selectionRectForText(RenderBlock::constructTextRun(renderer(), font, m_str, style, TextRun::AllowTrailingExpansion), IntPoint(x(), y() + root()->selectionTopAdjustedForPrecedingBlock()), root()->selectionHeightAdjustedForPrecedingBlock()));
}

void EllipsisBox::paintSelection(GraphicsContext* context, const LayoutPoint& paintOffset, RenderStyle* style, const Font& font)
{
    Color textColor = style->visitedDependentColor(CSSPropertyColor);
    Color c = m_renderer->selectionBackgroundColor();
    if (!c.isValid() || !c.alpha())
        return;

    // If the text color ends up being the same as the selection background, invert the selection
    // background.
    if (textColor == c)
        c = Color(0xff - c.red(), 0xff - c.green(), 0xff - c.blue());

    GraphicsContextStateSaver stateSaver(*context);
    LayoutUnit top = root()->selectionTop();
    LayoutUnit h = root()->selectionHeight();
    // FIXME: We'll need to apply the correct clip rounding here: https://bugs.webkit.org/show_bug.cgi?id=63656
    context->clip(IntRect(x() + paintOffset.x(), top + paintOffset.y(), m_logicalWidth, h));
    // FIXME: Why is this always LTR? Fix by passing correct text run flags below.
    context->drawHighlightForText(font, RenderBlock::constructTextRun(renderer(), font, m_str, style, TextRun::AllowTrailingExpansion), IntPoint(x() + paintOffset.x(), y() + paintOffset.y() + top), h, c, style->colorSpace());
}

bool EllipsisBox::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, const LayoutPoint& pointInContainer, const LayoutPoint& accumulatedOffset, LayoutUnit lineTop, LayoutUnit lineBottom)
{
    LayoutPoint adjustedLocation = accumulatedOffset + roundedLayoutPoint(topLeft());

    // Hit test the markup box.
    if (m_markupBox) {
        RenderStyle* style = m_renderer->style(isFirstLineStyle());
        LayoutUnit mtx = adjustedLocation.x() + m_logicalWidth - m_markupBox->x();
        LayoutUnit mty = adjustedLocation.y() + style->fontMetrics().ascent() - (m_markupBox->y() + m_markupBox->renderer()->style(isFirstLineStyle())->fontMetrics().ascent());
        if (m_markupBox->nodeAtPoint(request, result, pointInContainer, LayoutPoint(mtx, mty), lineTop, lineBottom)) {
            renderer()->updateHitTestResult(result, pointInContainer - LayoutSize(mtx, mty));
            return true;
        }
    }

    LayoutRect boundsRect(adjustedLocation, LayoutSize(m_logicalWidth, m_height));
    if (visibleToHitTesting() && boundsRect.intersects(result.rectForPoint(pointInContainer))) {
        renderer()->updateHitTestResult(result, pointInContainer - toLayoutSize(adjustedLocation));
        if (!result.addNodeToRectBasedTestResult(renderer()->node(), pointInContainer, boundsRect))
            return true;
    }

    return false;
}

} // namespace WebCore
