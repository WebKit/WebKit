/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "TextBoxPainter.h"

#include "CompositionHighlight.h"
#include "Editor.h"
#include "GraphicsContext.h"
#include "LegacyInlineTextBox.h"
#include "PaintInfo.h"
#include "StyledMarkedText.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"

namespace WebCore {

TextBoxPainter::TextBoxPainter(const LegacyInlineTextBox& textBox, PaintInfo& paintInfo, const FloatRect& boxRect)
    : m_textBox(textBox)
    , m_paintInfo(paintInfo)
    , m_boxRect(boxRect)
{
    bool shouldRotate = !m_textBox.isHorizontal() && !m_textBox.combinedText();
    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_boxRect, Clockwise));
}

TextBoxPainter::~TextBoxPainter()
{
    bool shouldRotate = !m_textBox.isHorizontal() && !m_textBox.combinedText();
    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_boxRect, Counterclockwise));
}

void TextBoxPainter::paintCompositionBackground()
{
    auto selectableRange = m_textBox.selectableRange();

    auto& editor = m_textBox.renderer().frame().editor();

    if (!editor.compositionUsesCustomHighlights()) {
        auto [clampedStart, clampedEnd] = selectableRange.clamp(editor.compositionStart(), editor.compositionEnd());

        paintBackground(clampedStart, clampedEnd, CompositionHighlight::defaultCompositionFillColor);
        return;
    }

    for (auto& highlight : editor.customCompositionHighlights()) {
        if (highlight.endOffset <= m_textBox.start())
            continue;

        if (highlight.startOffset >= m_textBox.end())
            break;

        auto [clampedStart, clampedEnd] = selectableRange.clamp(highlight.startOffset, highlight.endOffset);

        paintBackground(clampedStart, clampedEnd, highlight.color, BackgroundStyle::Rounded);

        if (highlight.endOffset > m_textBox.end())
            break;
    }
}

void TextBoxPainter::paintBackground(const StyledMarkedText& markedText)
{
    paintBackground(markedText.startOffset, markedText.endOffset, markedText.style.backgroundColor);
}

void TextBoxPainter::paintBackground(unsigned startOffset, unsigned endOffset, const Color& color, BackgroundStyle backgroundStyle)
{
    if (startOffset >= endOffset)
        return;

    GraphicsContext& context = m_paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };
    updateGraphicsContext(context, TextPaintStyle { color }); // Don't draw text at all!

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own highlight.
    TextRun textRun = m_textBox.createTextRun();

    const LegacyRootInlineBox& rootBox = m_textBox.root();
    LayoutUnit selectionBottom = rootBox.selectionBottom();
    LayoutUnit selectionTop = rootBox.selectionTopAdjustedForPrecedingBlock();

    // Use same y positioning and height as for selection, so that when the selection and this subrange are on
    // the same word there are no pieces sticking out.
    LayoutUnit deltaY { m_textBox.renderer().style().isFlippedLinesWritingMode() ? selectionBottom - m_textBox.logicalBottom() : m_textBox.logicalTop() - selectionTop };
    LayoutUnit selectionHeight = std::max<LayoutUnit>(0, selectionBottom - selectionTop);

    LayoutRect selectionRect { LayoutUnit(m_boxRect.x()), LayoutUnit(m_boxRect.y() - deltaY), LayoutUnit(m_textBox.logicalWidth()), selectionHeight };
    m_textBox.lineFont().adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);

    // FIXME: Support painting combined text. See <https://bugs.webkit.org/show_bug.cgi?id=180993>.
    auto backgroundRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_textBox.renderer().document().deviceScaleFactor(), textRun.ltr());
    if (backgroundStyle == BackgroundStyle::Rounded) {
        backgroundRect.expand(-1, -1);
        backgroundRect.move(0.5, 0.5);
        context.fillRoundedRect(FloatRoundedRect { backgroundRect, FloatRoundedRect::Radii { 2 } }, color);
        return;
    }

    context.fillRect(backgroundRect, color);
}

void TextBoxPainter::paintForeground(const StyledMarkedText& markedText)
{
    if (markedText.startOffset >= markedText.endOffset)
        return;

    GraphicsContext& context = m_paintInfo.context();
    const FontCascade& font = m_textBox.lineFont();
    const RenderStyle& lineStyle = m_textBox.lineStyle();

    float emphasisMarkOffset = 0;
    std::optional<bool> markExistsAndIsAbove = m_textBox.emphasisMarkExistsAndIsAbove(lineStyle);
    const AtomString& emphasisMark = markExistsAndIsAbove ? lineStyle.textEmphasisMarkString() : nullAtom();
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = *markExistsAndIsAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context };
    textPainter.setFont(font);
    textPainter.setStyle(markedText.style.textStyles);
    textPainter.setIsHorizontal(m_textBox.isHorizontal());
    if (markedText.style.textShadow) {
        textPainter.setShadow(&markedText.style.textShadow.value());
        if (lineStyle.hasAppleColorFilter())
            textPainter.setShadowColorFilter(&lineStyle.appleColorFilter());
    }
    textPainter.setEmphasisMark(emphasisMark, emphasisMarkOffset, m_textBox.combinedText());
    if (auto* debugShadow = m_textBox.debugTextShadow())
        textPainter.setShadow(debugShadow);

    TextRun textRun = m_textBox.createTextRun();
    textPainter.setGlyphDisplayListIfNeeded(m_textBox, m_paintInfo, font, context, textRun);

    GraphicsContextStateSaver stateSaver { context, false };
    if (markedText.type == MarkedText::DraggedContent) {
        stateSaver.save();
        context.setAlpha(markedText.style.alpha);
    }
    // TextPainter wants the box rectangle and text origin of the entire line box.
    textPainter.paintRange(textRun, m_boxRect, m_textBox.textOriginFromBoxRect(m_boxRect), markedText.startOffset, markedText.endOffset);
}

void TextBoxPainter::paintDecoration(const StyledMarkedText& markedText, const FloatRect& clipOutRect)
{
    if (m_textBox.truncation() == cFullTruncation)
        return;

    GraphicsContext& context = m_paintInfo.context();
    const FontCascade& font = m_textBox.lineFont();
    const RenderStyle& lineStyle = m_textBox.lineStyle();

    updateGraphicsContext(context, markedText.style.textStyles);

    bool isCombinedText = m_textBox.combinedText();
    if (isCombinedText)
        context.concatCTM(rotation(m_boxRect, Clockwise));

    // 1. Compute text selection
    unsigned startOffset = markedText.startOffset;
    unsigned endOffset = markedText.endOffset;
    if (startOffset >= endOffset)
        return;

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own decoration.
    TextRun textRun = m_textBox.createTextRun();

    // Avoid measuring the text when the entire line box is selected as an optimization.
    FloatRect snappedSelectionRect = m_boxRect;
    if (startOffset || endOffset != textRun.length()) {
        LayoutRect selectionRect = { m_boxRect.x(), m_boxRect.y(), m_boxRect.width(), m_boxRect.height() };
        font.adjustSelectionRectForText(textRun, selectionRect, startOffset, endOffset);
        snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_textBox.renderer().document().deviceScaleFactor(), textRun.ltr());
    }

    // 2. Paint
    auto textDecorations = lineStyle.textDecorationsInEffect();
    textDecorations.add(TextDecorationPainter::textDecorationsInEffectForStyle(markedText.style.textDecorationStyles));
    TextDecorationPainter decorationPainter { context, textDecorations, m_textBox.renderer(), m_textBox.isFirstLine(), font, markedText.style.textDecorationStyles };
    decorationPainter.setTextRunIterator(LayoutIntegration::textRunFor(&m_textBox));
    decorationPainter.setWidth(snappedSelectionRect.width());
    decorationPainter.setIsHorizontal(m_textBox.isHorizontal());
    if (markedText.style.textShadow) {
        decorationPainter.setTextShadow(&markedText.style.textShadow.value());
        if (lineStyle.hasAppleColorFilter())
            decorationPainter.setShadowColorFilter(&lineStyle.appleColorFilter());
    }

    {
        GraphicsContextStateSaver stateSaver { context, false };
        bool isDraggedContent = markedText.type == MarkedText::DraggedContent;
        if (isDraggedContent || !clipOutRect.isEmpty()) {
            stateSaver.save();
            if (isDraggedContent)
                context.setAlpha(markedText.style.alpha);
            if (!clipOutRect.isEmpty())
                context.clipOut(clipOutRect);
        }
        decorationPainter.paintTextDecoration(textRun.subRun(startOffset, endOffset - startOffset), m_textBox.textOriginFromBoxRect(snappedSelectionRect), snappedSelectionRect.location());
    }

    if (isCombinedText)
        context.concatCTM(rotation(m_boxRect, Counterclockwise));
}

void TextBoxPainter::paintCompositionUnderlines()
{
    if (m_textBox.truncation() == cFullTruncation)
        return;

    for (auto& underline : m_textBox.renderer().frame().editor().customCompositionUnderlines()) {
        if (underline.endOffset <= m_textBox.start()) {
            // Underline is completely before this run. This might be an underline that sits
            // before the first run we draw, or underlines that were within runs we skipped
            // due to truncation.
            continue;
        }

        if (underline.startOffset >= m_textBox.end())
            break; // Underline is completely after this run, bail. A later run will paint it.

        // Underline intersects this run. Paint it.
        paintCompositionUnderline(underline);

        if (underline.endOffset > m_textBox.end())
            break; // Underline also runs into the next run. Bail now, no more marker advancement.
    }
}

static inline void mirrorRTLSegment(float logicalWidth, TextDirection direction, float& start, float width)
{
    if (direction == TextDirection::LTR)
        return;
    start = logicalWidth - width - start;
}

void TextBoxPainter::paintCompositionUnderline(const CompositionUnderline& underline)
{
    auto& renderer = m_textBox.renderer();

    float start = 0; // start of line to draw, relative to tx
    float width = m_textBox.logicalWidth(); // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = m_textBox.start();
    unsigned paintEnd = m_textBox.end();
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = renderer.width(m_textBox.start(), paintStart - m_textBox.start(), m_textBox.textPos(), m_textBox.isFirstLine());
    }
    if (paintEnd != underline.endOffset) {
        paintEnd = std::min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_textBox.truncation() != cNoTruncation) {
        paintEnd = std::min(paintEnd, m_textBox.start() + m_textBox.truncation());
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = renderer.width(paintStart, paintEnd - paintStart, m_textBox.textPos() + start, m_textBox.isFirstLine());
        mirrorRTLSegment(m_textBox.logicalWidth(), m_textBox.direction(), start, width);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = m_textBox.lineStyle().fontMetrics().ascent();
    if (underline.thick && m_textBox.logicalHeight() - baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    GraphicsContext& context = m_paintInfo.context();
    Color underlineColor = underline.compositionUnderlineColor == CompositionUnderlineColor::TextColor ? renderer.style().visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor) : renderer.style().colorByApplyingColorFilter(underline.color);
    context.setStrokeColor(underlineColor);
    context.setStrokeThickness(lineThickness);
    context.drawLineForText(FloatRect(m_boxRect.x() + start, m_boxRect.y() + m_textBox.logicalHeight() - lineThickness, width, lineThickness), renderer.document().printing());
}

void TextBoxPainter::paintPlatformDocumentMarkers()
{
    auto markedTexts = MarkedText::collectForDocumentMarkers(m_textBox.renderer(), m_textBox.selectableRange(), MarkedText::PaintPhase::Decoration);
    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        paintPlatformDocumentMarker(markedText);
}

FloatRect TextBoxPainter::calculateUnionOfAllDocumentMarkerBounds(const LegacyInlineTextBox& textBox)
{
    // This must match paintPlatformDocumentMarkers().
    FloatRect result;
    auto markedTexts = MarkedText::collectForDocumentMarkers(textBox.renderer(), textBox.selectableRange(), MarkedText::PaintPhase::Decoration);
    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        result = unionRect(result, calculateDocumentMarkerBounds(textBox, markedText));
    return result;
}

void TextBoxPainter::paintPlatformDocumentMarker(const MarkedText& markedText)
{
    // Never print spelling/grammar markers (5327887)
    if (m_textBox.renderer().document().printing())
        return;

    if (m_textBox.truncation() == cFullTruncation)
        return;

    auto bounds = calculateDocumentMarkerBounds(m_textBox, markedText);

    auto lineStyleForMarkedTextType = [&]() -> DocumentMarkerLineStyle {
        bool shouldUseDarkAppearance = m_textBox.renderer().useDarkAppearance();
        switch (markedText.type) {
        case MarkedText::SpellingError:
            return { DocumentMarkerLineStyle::Mode::Spelling, shouldUseDarkAppearance };
        case MarkedText::GrammarError:
            return { DocumentMarkerLineStyle::Mode::Grammar, shouldUseDarkAppearance };
        case MarkedText::Correction:
            return { DocumentMarkerLineStyle::Mode::AutocorrectionReplacement, shouldUseDarkAppearance };
        case MarkedText::DictationAlternatives:
            return { DocumentMarkerLineStyle::Mode::DictationAlternatives, shouldUseDarkAppearance };
#if PLATFORM(IOS_FAMILY)
        case MarkedText::DictationPhraseWithAlternatives:
            // FIXME: Rename DocumentMarkerLineStyle::TextCheckingDictationPhraseWithAlternatives and remove the PLATFORM(IOS_FAMILY)-guard.
            return { DocumentMarkerLineStyle::Mode::TextCheckingDictationPhraseWithAlternatives, shouldUseDarkAppearance };
#endif
        default:
            ASSERT_NOT_REACHED();
            return { DocumentMarkerLineStyle::Mode::Spelling, shouldUseDarkAppearance };
        }
    };

    bounds.moveBy(m_boxRect.location());
    m_paintInfo.context().drawDotsForDocumentMarker(bounds, lineStyleForMarkedTextType());
}

FloatRect TextBoxPainter::calculateDocumentMarkerBounds(const LegacyInlineTextBox& textBox, const MarkedText& markedText)
{
    auto& font = textBox.lineFont();
    auto ascent = font.fontMetrics().ascent();
    auto fontSize = std::min(std::max(font.size(), 10.0f), 40.0f);
    auto y = ascent + 0.11035 * fontSize;
    auto height = 0.13247 * fontSize;

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (markedText.startOffset || markedText.endOffset != textBox.selectableRange().clamp(textBox.end())) {
        TextRun run = textBox.createTextRun();
        LayoutRect selectionRect = LayoutRect(0, y, 0, height);
        textBox.lineFont().adjustSelectionRectForText(run, selectionRect, markedText.startOffset, markedText.endOffset);
        return selectionRect;
    }

    return FloatRect(0, y, textBox.logicalWidth(), height);
}

}
