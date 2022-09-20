/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "EllipsisBoxPainter.h"

#include "InlineIteratorTextBox.h"
#include "LegacyEllipsisBox.h"
#include "LineSelection.h"
#include "PaintInfo.h"
#include "RenderView.h"

namespace WebCore {

EllipsisBoxPainter::EllipsisBoxPainter(const InlineIterator::LineBox& lineBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset, Color selectionForegroundColor, Color selectionBackgroundColor)
    : m_lineBox(lineBox)
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_selectionForegroundColor(selectionForegroundColor)
    , m_selectionBackgroundColor(selectionBackgroundColor)
{
}

void EllipsisBoxPainter::paint()
{
    // FIXME: Transition it to TextPainter.
    auto& context = m_paintInfo.context();
    auto& style = m_lineBox.style();
    auto textColor = style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor);

    if (m_lineBox.ellipsisSelectionState() != RenderObject::HighlightState::None) {
        paintSelection();

        // Select the correct color for painting the text.
        auto foreground = m_paintInfo.forceTextColor() ? m_paintInfo.forcedTextColor() : m_selectionForegroundColor;
        if (foreground.isValid() && foreground != textColor)
            context.setFillColor(foreground);
    }

    if (textColor != context.fillColor())
        context.setFillColor(textColor);

    auto setShadow = false;
    if (style.textShadow()) {
        auto shadowColor = style.colorByApplyingColorFilter(style.textShadow()->color());
        context.setShadow(LayoutSize(style.textShadow()->x().value(), style.textShadow()->y().value()), style.textShadow()->radius().value(), shadowColor);
        setShadow = true;
    }
    
    auto visualRect = m_lineBox.ellipsisVisualRect();
    auto textOrigin = visualRect.location();
    textOrigin.move(m_paintOffset.x(), m_paintOffset.y() + style.metricsOfPrimaryFont().ascent());
    context.drawBidiText(style.fontCascade(), m_lineBox.ellipsisText(), textOrigin);

    if (textColor != context.fillColor())
        context.setFillColor(textColor);

    if (setShadow)
        context.clearShadow();
}

void EllipsisBoxPainter::paintSelection()
{
    auto& context = m_paintInfo.context();
    auto& style = m_lineBox.style();

    auto textColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
    auto backgroundColor = m_selectionBackgroundColor;
    if (!backgroundColor.isVisible())
        return;

    // If the text color ends up being the same as the selection background, invert the selection background.
    if (textColor == backgroundColor)
        backgroundColor = backgroundColor.invertedColorWithAlpha(1.0);

    auto stateSaver = GraphicsContextStateSaver { context };

    auto visualRect = LayoutRect { m_lineBox.ellipsisVisualRect(InlineIterator::LineBox::AdjustedForSelection::Yes) };
    visualRect.move(m_paintOffset.x(), m_paintOffset.y());

    auto ellipsisText = m_lineBox.ellipsisText();
    style.fontCascade().adjustSelectionRectForText(ellipsisText, visualRect);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(visualRect, m_lineBox.formattingContextRoot().document().deviceScaleFactor(), ellipsisText.ltr()), backgroundColor);
}

}
