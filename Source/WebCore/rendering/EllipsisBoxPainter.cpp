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

#include "InlineIteratorLineBox.h"
#include "LegacyEllipsisBox.h"
#include "LineSelection.h"
#include "PaintInfo.h"

namespace WebCore {

LegacyEllipsisBoxPainter::LegacyEllipsisBoxPainter(const LegacyEllipsisBox& ellipsisBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset, Color selectionForegroundColor, Color selectionBackgroundColor)
    : EllipsisBoxPainter(InlineIterator::BoxLegacyPath { &ellipsisBox }, paintInfo, paintOffset, selectionForegroundColor, selectionBackgroundColor)
{
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
ModernEllipsisBoxPainter::ModernEllipsisBoxPainter(const LayoutIntegration::InlineContent& inlineContent, const InlineDisplay::Box& ellipsisBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset, Color selectionForegroundColor, Color selectionBackgroundColor)
    : EllipsisBoxPainter(InlineIterator::BoxModernPath { inlineContent, inlineContent.indexForBox(ellipsisBox) }, paintInfo, paintOffset, selectionForegroundColor, selectionBackgroundColor)
{
}
#endif

template<typename EllipsisBoxPath>
EllipsisBoxPainter<EllipsisBoxPath>::EllipsisBoxPainter(EllipsisBoxPath&& ellipsisBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset, Color selectionForegroundColor, Color selectionBackgroundColor)
    : m_ellipsisBox(WTFMove(ellipsisBox))
    , m_paintInfo(paintInfo)
    , m_paintOffset(paintOffset)
    , m_selectionForegroundColor(selectionForegroundColor)
    , m_selectionBackgroundColor(selectionBackgroundColor)
{
}

template<typename EllipsisBoxPath>
void EllipsisBoxPainter<EllipsisBoxPath>::paint()
{
    // FIXME: Transition it to TextPainter.
    auto& context = m_paintInfo.context();
    auto& style = m_ellipsisBox.style();
    auto textColor = style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor);

    if (selectionState() != RenderObject::HighlightState::None) {
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
    
    auto visualRect = m_ellipsisBox.visualRectIgnoringBlockDirection();
    m_ellipsisBox.containingBlock().flipForWritingMode(visualRect);
    auto textOrigin = visualRect.location();
    textOrigin.move(m_paintOffset.x(), m_paintOffset.y() + style.metricsOfPrimaryFont().ascent());
    context.drawBidiText(style.fontCascade(), m_ellipsisBox.textRun(), textOrigin);

    if (textColor != context.fillColor())
        context.setFillColor(textColor);

    if (setShadow)
        context.clearShadow();
}

template<typename EllipsisBoxPath>
void EllipsisBoxPainter<EllipsisBoxPath>::paintSelection()
{
    auto& context = m_paintInfo.context();
    auto& style = m_ellipsisBox.style();

    auto textColor = style.visitedDependentColorWithColorFilter(CSSPropertyColor);
    auto backgroundColor = m_selectionBackgroundColor;
    if (!backgroundColor.isVisible())
        return;

    // If the text color ends up being the same as the selection background, invert the selection background.
    if (textColor == backgroundColor)
        backgroundColor = backgroundColor.invertedColorWithAlpha(1.0);

    auto stateSaver = GraphicsContextStateSaver { context };

    auto visualRect = LayoutRect { m_ellipsisBox.visualRectIgnoringBlockDirection() };
    auto [selectionTop, selectionBottom] = selectionTopAndBottom();

    visualRect.setY(selectionTop);
    visualRect.setHeight(selectionBottom - selectionTop);

    m_ellipsisBox.containingBlock().flipForWritingMode(visualRect);
    visualRect.move(m_paintOffset.x(), m_paintOffset.y());

    auto textRun = m_ellipsisBox.textRun();
    style.fontCascade().adjustSelectionRectForText(textRun, visualRect);
    context.fillRect(snapRectToDevicePixelsWithWritingDirection(visualRect, m_ellipsisBox.renderer().document().deviceScaleFactor(), textRun.ltr()), backgroundColor);
}

template<typename EllipsisBoxPath>
std::tuple<float, float> EllipsisBoxPainter<EllipsisBoxPath>::selectionTopAndBottom() const
{
    auto pathCopy = m_ellipsisBox;
    auto lineBox = InlineIterator::LeafBoxIterator { WTFMove(pathCopy) }->lineBox();
    return { LineSelection::logicalTopAdjustedForPrecedingBlock(*lineBox), LineSelection::logicalBottom(*lineBox) };
}

template<typename EllipsisBoxPath>
RenderObject::HighlightState EllipsisBoxPainter<EllipsisBoxPath>::selectionState() const
{
    auto pathCopy = m_ellipsisBox;
    return InlineIterator::LeafBoxIterator { WTFMove(pathCopy) }->selectionState();
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
template class EllipsisBoxPainter<InlineIterator::BoxModernPath>;
#endif
template class EllipsisBoxPainter<InlineIterator::BoxLegacyPath>;

}
