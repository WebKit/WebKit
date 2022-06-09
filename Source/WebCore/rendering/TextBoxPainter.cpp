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
#include "EventRegion.h"
#include "GraphicsContext.h"
#include "InlineIteratorLine.h"
#include "LegacyInlineTextBox.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderCombineText.h"
#include "RenderText.h"
#include "RenderView.h"
#include "ShadowData.h"
#include "StyledMarkedText.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"

namespace WebCore {

TextBoxPainter::TextBoxPainter(const LegacyInlineTextBox& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : TextBoxPainter(InlineIterator::textBoxFor(&textBox), paintInfo, paintOffset)
{
    m_emphasisMarkExistsAndIsAbove = textBox.emphasisMarkExistsAndIsAbove(m_style);
}

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
TextBoxPainter::TextBoxPainter(const LayoutIntegration::InlineContent& inlineContent, const InlineDisplay::Box& box, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : TextBoxPainter(InlineIterator::textBoxFor(inlineContent, box), paintInfo, paintOffset)
{
}
#endif

TextBoxPainter::TextBoxPainter(const InlineIterator::TextBoxIterator& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_textBox(textBox)
    , m_renderer(m_textBox->renderer())
    , m_document(m_renderer.document())
    , m_style(m_textBox->style())
    , m_paintTextRun(m_textBox->createTextRun())
    , m_paintInfo(paintInfo)
    , m_selectableRange(m_textBox->selectableRange())
    , m_paintRect(computePaintRect(paintOffset))
    , m_isFirstLine(m_textBox->line()->isFirst())
    , m_isPrinting(m_document.printing())
    , m_haveSelection(computeHaveSelection())
    , m_containsComposition(m_renderer.textNode() && m_renderer.frame().editor().compositionNode() == m_renderer.textNode())
    , m_useCustomUnderlines(m_containsComposition && m_renderer.frame().editor().compositionUsesCustomUnderlines())
{
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection || paintInfo.phase == PaintPhase::TextClip || paintInfo.phase == PaintPhase::EventRegion);
}

TextBoxPainter::~TextBoxPainter()
{
}

void TextBoxPainter::paint()
{
    if (m_paintInfo.phase == PaintPhase::Selection && !m_haveSelection)
        return;

    if (m_paintInfo.phase == PaintPhase::EventRegion) {
        if (m_renderer.parent()->visibleToHitTesting())
            m_paintInfo.eventRegionContext->unite(enclosingIntRect(m_paintRect), m_style);
        return;
    }

    bool shouldRotate = !textBox().isHorizontal() && !textBox().isCombinedText();
    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Clockwise));

    if (m_paintInfo.phase == PaintPhase::Foreground) {
        if (!m_isPrinting)
            paintBackground();

        paintPlatformDocumentMarkers();
    }

    paintForegroundAndDecorations();

    if (m_paintInfo.phase == PaintPhase::Foreground) {
        if (m_useCustomUnderlines)
            paintCompositionUnderlines();

        m_renderer.page().addRelevantRepaintedObject(const_cast<RenderText*>(&m_renderer), enclosingLayoutRect(m_paintRect));
    }

    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Counterclockwise));
}

MarkedText TextBoxPainter::createMarkedTextFromSelectionInBox()
{
    auto [selectionStart, selectionEnd] = m_renderer.view().selection().rangeForTextBox(m_renderer, m_selectableRange);
    if (selectionStart < selectionEnd)
        return { selectionStart, selectionEnd, MarkedText::Selection };
    return { };
}

void TextBoxPainter::paintBackground()
{
    if (m_containsComposition && !m_useCustomUnderlines)
        paintCompositionBackground();

    Vector<MarkedText> markedTexts;
    markedTexts.appendVector(MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Background));
    markedTexts.appendVector(MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Background));

#if ENABLE(TEXT_SELECTION)
    if (m_haveSelection && !m_useCustomUnderlines && !m_paintInfo.context().paintingDisabled()) {
        auto selectionMarkedText = createMarkedTextFromSelectionInBox();
        if (!selectionMarkedText.isEmpty())
            markedTexts.append(WTFMove(selectionMarkedText));
    }
#endif
    auto styledMarkedTexts = StyledMarkedText::subdivideAndResolve(markedTexts, m_renderer, m_isFirstLine, m_paintInfo);

    // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
    auto coalescedStyledMarkedTexts = StyledMarkedText::coalesceAdjacentWithEqualBackground(styledMarkedTexts);

    for (auto& markedText : coalescedStyledMarkedTexts)
        paintBackground(markedText);
}

void TextBoxPainter::paintForegroundAndDecorations()
{
    bool shouldPaintSelectionForeground = m_haveSelection && !m_useCustomUnderlines;
    Vector<MarkedText> markedTexts;
    if (m_paintInfo.phase != PaintPhase::Selection) {
        // The marked texts for the gaps between document markers and selection are implicitly created by subdividing the entire line.
        markedTexts.append({ m_selectableRange.clamp(textBox().start()), m_selectableRange.clamp(textBox().end()), MarkedText::Unmarked });

        if (!m_isPrinting) {
            markedTexts.appendVector(MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Foreground));
            markedTexts.appendVector(MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Foreground));

            bool shouldPaintDraggedContent = !(m_paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection));
            if (shouldPaintDraggedContent) {
                auto markedTextsForDraggedContent = MarkedText::collectForDraggedContent(m_renderer, m_selectableRange);
                if (!markedTextsForDraggedContent.isEmpty()) {
                    shouldPaintSelectionForeground = false;
                    markedTexts.appendVector(markedTextsForDraggedContent);
                }
            }
        }
    }
    // The selection marked text acts as a placeholder when computing the marked texts for the gaps...
    if (shouldPaintSelectionForeground) {
        ASSERT(!m_isPrinting);
        auto selectionMarkedText = createMarkedTextFromSelectionInBox();
        if (!selectionMarkedText.isEmpty())
            markedTexts.append(WTFMove(selectionMarkedText));
    }

    auto styledMarkedTexts = StyledMarkedText::subdivideAndResolve(markedTexts, m_renderer, m_isFirstLine, m_paintInfo);

    // ... now remove the selection marked text if we are excluding selection.
    if (!m_isPrinting && m_paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection)) {
        styledMarkedTexts.removeAllMatching([] (const StyledMarkedText& markedText) {
            return markedText.type == MarkedText::Selection;
        });
    }

    // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
    auto coalescedStyledMarkedTexts = StyledMarkedText::coalesceAdjacentWithEqualForeground(styledMarkedTexts);

    auto textDecorations = m_style.textDecorationsInEffect();
    bool highlightDecorations = !MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration).isEmpty();
    bool lineDecorations = !textDecorations.isEmpty();
    if ((lineDecorations || highlightDecorations) && m_paintInfo.phase != PaintPhase::Selection) {
        TextRun textRun = textBox().createTextRun();
        unsigned length = m_selectableRange.truncation.value_or(textRun.length());
        unsigned selectionStart = 0;
        unsigned selectionEnd = 0;
        if (m_haveSelection)
            std::tie(selectionStart, selectionEnd) = m_renderer.view().selection().rangeForTextBox(m_renderer, m_selectableRange);

        FloatRect textDecorationSelectionClipOutRect;
        if ((m_paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection)) && selectionStart < selectionEnd && selectionEnd <= length) {
            textDecorationSelectionClipOutRect = m_paintRect;
            float logicalWidthBeforeRange;
            float logicalWidthAfterRange;
            float logicalSelectionWidth = fontCascade().widthOfTextRange(m_paintTextRun, selectionStart, selectionEnd, nullptr, &logicalWidthBeforeRange, &logicalWidthAfterRange);
            // FIXME: Do we need to handle vertical bottom to top text?
            if (!textBox().isHorizontal()) {
                textDecorationSelectionClipOutRect.move(0, logicalWidthBeforeRange);
                textDecorationSelectionClipOutRect.setHeight(logicalSelectionWidth);
            } else if (textBox().direction() == TextDirection::RTL) {
                textDecorationSelectionClipOutRect.move(logicalWidthAfterRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            } else {
                textDecorationSelectionClipOutRect.move(logicalWidthBeforeRange, 0);
                textDecorationSelectionClipOutRect.setWidth(logicalSelectionWidth);
            }
        }

        // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
        auto coalescedStyledMarkedTexts = StyledMarkedText::coalesceAdjacentWithEqualDecorations(styledMarkedTexts);

        for (auto& markedText : coalescedStyledMarkedTexts) {
            unsigned startOffset = markedText.startOffset;
            unsigned endOffset = markedText.endOffset;
            if (startOffset < endOffset) {
                // Avoid measuring the text when the entire line box is selected as an optimization.
                FloatRect snappedSelectionRect = m_paintRect;
                if (startOffset || endOffset != m_paintTextRun.length()) {
                    LayoutRect selectionRect = { m_paintRect.x(), m_paintRect.y(), m_paintRect.width(), m_paintRect.height() };
                    fontCascade().adjustSelectionRectForText(m_paintTextRun, selectionRect, startOffset, endOffset);
                    snappedSelectionRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_document.deviceScaleFactor(), m_paintTextRun.ltr());
                }

                TextDecorationPainter decorationPainter = createDecorationPainter(markedText, textDecorationSelectionClipOutRect, snappedSelectionRect);
                paintBackgroundDecorations(decorationPainter, markedText, snappedSelectionRect);
                paintForeground(markedText);
                paintForegroundDecorations(decorationPainter, snappedSelectionRect);
            }
        }
    } else {
        for (auto& markedText : coalescedStyledMarkedTexts)
            paintForeground(markedText);
    }
}

void TextBoxPainter::paintCompositionBackground()
{
    auto& editor = m_renderer.frame().editor();

    if (!editor.compositionUsesCustomHighlights()) {
        auto [clampedStart, clampedEnd] = m_selectableRange.clamp(editor.compositionStart(), editor.compositionEnd());

        paintBackground(clampedStart, clampedEnd, CompositionHighlight::defaultCompositionFillColor);
        return;
    }

    for (auto& highlight : editor.customCompositionHighlights()) {
        if (highlight.endOffset <= textBox().start())
            continue;

        if (highlight.startOffset >= textBox().end())
            break;

        auto [clampedStart, clampedEnd] = m_selectableRange.clamp(highlight.startOffset, highlight.endOffset);

        paintBackground(clampedStart, clampedEnd, highlight.color, BackgroundStyle::Rounded);

        if (highlight.endOffset > textBox().end())
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

    const auto line = textBox().line();
    LayoutUnit selectionBottom = line->selectionBottom();
    LayoutUnit selectionTop = line->selectionTopAdjustedForPrecedingBlock();

    // Use same y positioning and height as for selection, so that when the selection and this subrange are on
    // the same word there are no pieces sticking out.
    LayoutUnit deltaY { m_style.isFlippedLinesWritingMode() ? selectionBottom - textBox().logicalBottom() : textBox().logicalTop() - selectionTop };
    LayoutUnit selectionHeight = std::max<LayoutUnit>(0, selectionBottom - selectionTop);

    LayoutRect selectionRect { LayoutUnit(m_paintRect.x()), LayoutUnit(m_paintRect.y() - deltaY), LayoutUnit(textBox().logicalWidth()), selectionHeight };
    fontCascade().adjustSelectionRectForText(m_paintTextRun, selectionRect, startOffset, endOffset);

    // FIXME: Support painting combined text. See <https://bugs.webkit.org/show_bug.cgi?id=180993>.
    auto backgroundRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_document.deviceScaleFactor(), m_paintTextRun.ltr());
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
    const FontCascade& font = fontCascade();

    float emphasisMarkOffset = 0;
    const AtomString& emphasisMark = m_emphasisMarkExistsAndIsAbove ? m_style.textEmphasisMarkString() : nullAtom();
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = *m_emphasisMarkExistsAndIsAbove ? -font.fontMetrics().ascent() - font.emphasisMarkDescent(emphasisMark) : font.fontMetrics().descent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context };
    textPainter.setFont(font);
    textPainter.setStyle(markedText.style.textStyles);
    textPainter.setIsHorizontal(textBox().isHorizontal());
    if (markedText.style.textShadow) {
        textPainter.setShadow(&markedText.style.textShadow.value());
        if (m_style.hasAppleColorFilter())
            textPainter.setShadowColorFilter(&m_style.appleColorFilter());
    }
    textPainter.setEmphasisMark(emphasisMark, emphasisMarkOffset, textBox().isCombinedText() ? &downcast<RenderCombineText>(m_renderer) : nullptr);
    if (auto* debugShadow = debugTextShadow())
        textPainter.setShadow(debugShadow);

    if (auto* legacyInlineBox = textBox().legacyInlineBox())
        textPainter.setGlyphDisplayListIfNeeded(*legacyInlineBox, m_paintInfo, font, context, m_paintTextRun);
#if ENABLE(LAYOUT_FORMATTING_CONTEXT)
    else
        textPainter.setGlyphDisplayListIfNeeded(*textBox().inlineBox(), m_paintInfo, font, context, m_paintTextRun);
#endif

    GraphicsContextStateSaver stateSaver { context, false };
    if (markedText.type == MarkedText::DraggedContent) {
        stateSaver.save();
        context.setAlpha(markedText.style.alpha);
    }
    // TextPainter wants the box rectangle and text origin of the entire line box.
    textPainter.paintRange(m_paintTextRun, m_paintRect, textOriginFromPaintRect(m_paintRect), markedText.startOffset, markedText.endOffset);
}

TextDecorationPainter TextBoxPainter::createDecorationPainter(const StyledMarkedText& markedText, const FloatRect& clipOutRect, const FloatRect& snappedSelectionRect)
{
    GraphicsContext& context = m_paintInfo.context();

    updateGraphicsContext(context, markedText.style.textStyles);

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own decoration.
    GraphicsContextStateSaver stateSaver { context, false };
    bool isDraggedContent = markedText.type == MarkedText::DraggedContent;
    if (isDraggedContent || !clipOutRect.isEmpty()) {
        stateSaver.save();
        if (isDraggedContent)
            context.setAlpha(markedText.style.alpha);
        if (!clipOutRect.isEmpty())
            context.clipOut(clipOutRect);
    }

    // Create painter
    const FontCascade& font = fontCascade();
    auto textDecorations = m_style.textDecorationsInEffect();
    textDecorations.add(TextDecorationPainter::textDecorationsInEffectForStyle(markedText.style.textDecorationStyles));
    TextDecorationPainter decorationPainter { context, textDecorations, m_renderer, m_isFirstLine, font, markedText.style.textDecorationStyles };
    decorationPainter.setTextBox(m_textBox);
    decorationPainter.setWidth(snappedSelectionRect.width());
    decorationPainter.setIsHorizontal(textBox().isHorizontal());
    if (markedText.style.textShadow) {
        decorationPainter.setTextShadow(&markedText.style.textShadow.value());
        if (m_style.hasAppleColorFilter())
            decorationPainter.setShadowColorFilter(&m_style.appleColorFilter());
    }

    return decorationPainter;
}

void TextBoxPainter::paintBackgroundDecorations(TextDecorationPainter& decorationPainter, const StyledMarkedText& markedText, const FloatRect& snappedSelectionRect)
{
    bool isCombinedText = textBox().isCombinedText();
    if (isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Clockwise));

    decorationPainter.paintBackgroundDecorations(m_paintTextRun.subRun(markedText.startOffset, markedText.endOffset - markedText.startOffset), textOriginFromPaintRect(snappedSelectionRect), snappedSelectionRect.location());

    if (isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Counterclockwise));
}

void TextBoxPainter::paintForegroundDecorations(TextDecorationPainter& decorationPainter, const FloatRect& snappedSelectionRect)
{
    bool isCombinedText = textBox().isCombinedText();
    if (isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Clockwise));

    decorationPainter.paintForegroundDecorations(snappedSelectionRect.location());

    if (isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Counterclockwise));
}

void TextBoxPainter::paintCompositionUnderlines()
{
    for (auto& underline : m_renderer.frame().editor().customCompositionUnderlines()) {
        if (underline.endOffset <= textBox().start()) {
            // Underline is completely before this run. This might be an underline that sits
            // before the first run we draw, or underlines that were within runs we skipped
            // due to truncation.
            continue;
        }

        if (underline.startOffset >= textBox().end())
            break; // Underline is completely after this run, bail. A later run will paint it.

        // Underline intersects this run. Paint it.
        paintCompositionUnderline(underline);

        if (underline.endOffset > textBox().end())
            break; // Underline also runs into the next run. Bail now, no more marker advancement.
    }
}

static inline void mirrorRTLSegment(float logicalWidth, TextDirection direction, float& start, float width)
{
    if (direction == TextDirection::LTR)
        return;
    start = logicalWidth - width - start;
}

static float textPosition(const InlineIterator::TextBoxIterator& textBox)
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPosition() should be consistent so the text are rendered in the same width.
    if (!textBox->logicalLeft())
        return 0;
    return textBox->logicalLeft() - textBox->line()->contentLogicalLeft();
}

void TextBoxPainter::paintCompositionUnderline(const CompositionUnderline& underline)
{
    float start = 0; // start of line to draw, relative to tx
    float width = textBox().logicalWidth(); // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = textBox().start();
    unsigned paintEnd = textBox().end();
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = m_renderer.width(textBox().start(), paintStart - textBox().start(), textPosition(m_textBox), m_isFirstLine);
    }
    if (paintEnd != underline.endOffset) {
        paintEnd = std::min(paintEnd, (unsigned)underline.endOffset);
        useWholeWidth = false;
    }
    if (m_selectableRange.truncation) {
        paintEnd = std::min(paintEnd, textBox().start() + *m_selectableRange.truncation);
        useWholeWidth = false;
    }
    if (!useWholeWidth) {
        width = m_renderer.width(paintStart, paintEnd - paintStart, textPosition(m_textBox) + start, m_isFirstLine);
        mirrorRTLSegment(textBox().logicalWidth(), textBox().direction(), start, width);
    }

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = m_style.fontMetrics().ascent();
    if (underline.thick && textBox().logicalHeight() - baseline >= 2)
        lineThickness = 2;

    // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
    // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
    start += 1;
    width -= 2;

    auto& style = m_renderer.style();
    Color underlineColor = underline.compositionUnderlineColor == CompositionUnderlineColor::TextColor ? style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor) : style.colorByApplyingColorFilter(underline.color);

    GraphicsContext& context = m_paintInfo.context();
    context.setStrokeColor(underlineColor);
    context.setStrokeThickness(lineThickness);
    context.drawLineForText(FloatRect(m_paintRect.x() + start, m_paintRect.y() + textBox().logicalHeight() - lineThickness, width, lineThickness), m_document.printing());
}

void TextBoxPainter::paintPlatformDocumentMarkers()
{
    auto markedTexts = MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration);
    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        paintPlatformDocumentMarker(markedText);
}

FloatRect TextBoxPainter::calculateUnionOfAllDocumentMarkerBounds(const LegacyInlineTextBox& textBox)
{
    // This must match paintPlatformDocumentMarkers().
    FloatRect result;
    auto markedTexts = MarkedText::collectForDocumentMarkers(textBox.renderer(), textBox.selectableRange(), MarkedText::PaintPhase::Decoration);
    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        result = unionRect(result, calculateDocumentMarkerBounds(InlineIterator::textBoxFor(&textBox), markedText));
    return result;
}

void TextBoxPainter::paintPlatformDocumentMarker(const MarkedText& markedText)
{
    // Never print spelling/grammar markers (5327887)
    if (m_document.printing())
        return;

    auto bounds = calculateDocumentMarkerBounds(m_textBox, markedText);

    auto lineStyleForMarkedTextType = [&]() -> DocumentMarkerLineStyle {
        bool shouldUseDarkAppearance = m_renderer.useDarkAppearance();
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

    bounds.moveBy(m_paintRect.location());
    m_paintInfo.context().drawDotsForDocumentMarker(bounds, lineStyleForMarkedTextType());
}

FloatRect TextBoxPainter::computePaintRect(const LayoutPoint& paintOffset)
{
    FloatPoint localPaintOffset(paintOffset);

    if (m_selectableRange.truncation) {
        if (m_renderer.containingBlock()->style().isLeftToRightDirection() != textBox().isLeftToRightDirection()) {
            // Make the visible fragment of text hug the edge closest to the rest of the run by moving the origin
            // at which we start drawing text.
            // e.g. In the case of LTR text truncated in an RTL Context, the correct behavior is:
            // |Hello|CBA| -> |...He|CBA|
            // In order to draw the fragment "He" aligned to the right edge of it's box, we need to start drawing
            // farther to the right.
            // NOTE: WebKit's behavior differs from that of IE which appears to just overlay the ellipsis on top of the
            // truncated string i.e.  |Hello|CBA| -> |...lo|CBA|
            LayoutUnit widthOfVisibleText { m_renderer.width(textBox().start(), *m_selectableRange.truncation, textPosition(m_textBox), m_isFirstLine) };
            LayoutUnit widthOfHiddenText { textBox().logicalWidth() - widthOfVisibleText };
            LayoutSize truncationOffset(textBox().isLeftToRightDirection() ? widthOfHiddenText : -widthOfHiddenText, 0_lu);
            localPaintOffset.move(textBox().isHorizontal() ? truncationOffset : truncationOffset.transposedSize());
        }
    }

    localPaintOffset.move(0, m_style.isHorizontalWritingMode() ? 0 : -textBox().logicalHeight());

    auto locationIncludingFlipping = [&]() -> FloatPoint {
        auto rect = textBox().rect();
        if (!m_style.isFlippedBlocksWritingMode())
            return rect.location();
        auto& block = textBox().line()->containingBlock();
        if (block.style().isHorizontalWritingMode())
            return { rect.x(), block.height() - rect.height() - rect.y() };
        return { block.width() - rect.width() - rect.x(), rect.y() };
    };

    FloatPoint boxOrigin = locationIncludingFlipping();
    boxOrigin.moveBy(localPaintOffset);
    return { boxOrigin, FloatSize(textBox().logicalWidth(), textBox().logicalHeight()) };
}

FloatRect TextBoxPainter::calculateDocumentMarkerBounds(const InlineIterator::TextBoxIterator& textBox, const MarkedText& markedText)
{
    auto& font = textBox->fontCascade();
    auto ascent = font.fontMetrics().ascent();
    auto fontSize = std::min(std::max(font.size(), 10.0f), 40.0f);
    auto y = ascent + 0.11035 * fontSize;
    auto height = 0.13247 * fontSize;

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (markedText.startOffset || markedText.endOffset != textBox->selectableRange().clamp(textBox->end())) {
        TextRun run = textBox->createTextRun();
        LayoutRect selectionRect = LayoutRect(0, y, 0, height);
        font.adjustSelectionRectForText(run, selectionRect, markedText.startOffset, markedText.endOffset);
        return selectionRect;
    }

    return FloatRect(0, y, textBox->logicalWidth(), height);
}

bool TextBoxPainter::computeHaveSelection() const
{
    if (m_isPrinting || m_paintInfo.phase == PaintPhase::TextClip)
        return false;

    return m_renderer.view().selection().highlightStateForTextBox(m_renderer, m_selectableRange) != RenderObject::HighlightState::None;
}

const FontCascade& TextBoxPainter::fontCascade() const
{
    return m_textBox->fontCascade();
}

FloatPoint TextBoxPainter::textOriginFromPaintRect(const FloatRect& paintRect) const
{
    FloatPoint textOrigin { paintRect.x(), paintRect.y() + fontCascade().fontMetrics().ascent() };
    if (textBox().isCombinedText()) {
        if (auto newOrigin = downcast<RenderCombineText>(m_renderer).computeTextOrigin(paintRect))
            textOrigin = newOrigin.value();
    }
    if (textBox().isHorizontal())
        textOrigin.setY(roundToDevicePixel(LayoutUnit { textOrigin.y() }, m_renderer.document().deviceScaleFactor()));
    else
        textOrigin.setX(roundToDevicePixel(LayoutUnit { textOrigin.x() }, m_renderer.document().deviceScaleFactor()));
    return textOrigin;
}

const ShadowData* TextBoxPainter::debugTextShadow() const
{
    if (!m_renderer.settings().legacyLineLayoutVisualCoverageEnabled())
        return nullptr;
    if (!textBox().legacyInlineBox())
        return nullptr;

    static NeverDestroyed<ShadowData> debugTextShadow(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(10, LengthType::Fixed), Length(20, LengthType::Fixed), ShadowStyle::Normal, true, SRGBA<uint8_t> { 150, 0, 0, 190 });
    return &debugTextShadow.get();
}


}
