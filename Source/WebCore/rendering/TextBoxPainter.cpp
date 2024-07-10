/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "EventRegion.h"
#include "GraphicsContext.h"
#include "HTMLAnchorElement.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorLineBox.h"
#include "InlineIteratorTextBoxInlines.h"
#include "InlineTextBoxStyle.h"
#include "LegacyInlineTextBox.h"
#include "LineSelection.h"
#include "PaintInfo.h"
#include "RenderBlock.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderCombineText.h"
#include "RenderElementInlines.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderedDocumentMarker.h"
#include "ShadowData.h"
#include "StyledMarkedText.h"
#include "TextDecorationThickness.h"
#include "TextPaintStyle.h"
#include "TextPainter.h"

#if ENABLE(WRITING_TOOLS)
#include "GraphicsContextCG.h"
#endif

namespace WebCore {

static FloatRect calculateDocumentMarkerBounds(const InlineIterator::TextBoxIterator&, const MarkedText&);

LegacyTextBoxPainter::LegacyTextBoxPainter(const LegacyInlineTextBox& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : TextBoxPainter(InlineIterator::BoxLegacyPath { &textBox }, paintInfo, paintOffset)
{
}

ModernTextBoxPainter::ModernTextBoxPainter(const LayoutIntegration::InlineContent& inlineContent, const InlineDisplay::Box& box, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : TextBoxPainter(InlineIterator::BoxModernPath { inlineContent, inlineContent.indexForBox(box) }, paintInfo, paintOffset)
{
}

template<typename TextBoxPath>
TextBoxPainter<TextBoxPath>::TextBoxPainter(TextBoxPath&& textBox, PaintInfo& paintInfo, const LayoutPoint& paintOffset)
    : m_textBox(WTFMove(textBox))
    , m_renderer(downcast<RenderText>(m_textBox.renderer()))
    , m_document(m_renderer.document())
    , m_style(m_textBox.style())
    , m_logicalRect(m_textBox.isHorizontal() ? m_textBox.visualRectIgnoringBlockDirection() : m_textBox.visualRectIgnoringBlockDirection().transposedRect())
    , m_paintTextRun(m_textBox.textRun())
    , m_paintInfo(paintInfo)
    , m_selectableRange(m_textBox.selectableRange())
    , m_paintOffset(paintOffset)
    , m_paintRect(computePaintRect(paintOffset))
    , m_isFirstLine(m_textBox.isFirstLine())
    , m_isCombinedText([&] {
        auto* combineTextRenderer = dynamicDowncast<RenderCombineText>(m_renderer);
        return combineTextRenderer && combineTextRenderer->isCombined();
    }())
    , m_isPrinting(m_document.printing())
    , m_haveSelection(computeHaveSelection())
    , m_containsComposition(m_renderer.textNode() && m_renderer.frame().editor().compositionNode() == m_renderer.textNode())
    , m_useCustomUnderlines(m_containsComposition && m_renderer.frame().editor().compositionUsesCustomUnderlines())
    , m_emphasisMarkExistsAndIsAbove(RenderText::emphasisMarkExistsAndIsAbove(m_renderer, m_style))
{
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection || paintInfo.phase == PaintPhase::TextClip || paintInfo.phase == PaintPhase::EventRegion || paintInfo.phase == PaintPhase::Accessibility);
}

template<typename TextBoxPath>
TextBoxPainter<TextBoxPath>::~TextBoxPainter()
{
}

template<typename TextBoxPath>
InlineIterator::TextBoxIterator TextBoxPainter<TextBoxPath>::makeIterator() const
{
    auto pathCopy = m_textBox;
    return InlineIterator::TextBoxIterator { WTFMove(pathCopy) };
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paint()
{
    if (m_paintInfo.phase == PaintPhase::Selection && !m_haveSelection)
        return;

    if (m_paintInfo.phase == PaintPhase::EventRegion) {
        constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::IgnoreCSSPointerEventsProperty };
        if (m_renderer.parent()->visibleToHitTesting(hitType))
            m_paintInfo.eventRegionContext()->unite(FloatRoundedRect(m_paintRect), const_cast<RenderText&>(m_renderer), m_style);
        return;
    }

    if (m_paintInfo.phase == PaintPhase::Accessibility) {
        m_paintInfo.accessibilityRegionContext()->takeBounds(m_renderer, m_paintRect);
        return;
    }

    bool shouldRotate = !textBox().isHorizontal() && !m_isCombinedText;
    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Clockwise));

    if (m_paintInfo.phase == PaintPhase::Foreground) {
        if (!m_isPrinting)
            paintBackground();

        paintPlatformDocumentMarkers();
    }

    paintForegroundAndDecorations();

    if (m_paintInfo.phase == PaintPhase::Foreground) {
        if (m_useCustomUnderlines)
            paintCompositionUnderlines();

        m_renderer.page().addRelevantRepaintedObject(m_renderer, enclosingLayoutRect(m_paintRect));
    }

    if (shouldRotate)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Counterclockwise));
}

template<typename TextBoxPath>
MarkedText TextBoxPainter<TextBoxPath>::createMarkedTextFromSelectionInBox()
{
    auto [selectionStart, selectionEnd] = m_renderer.view().selection().rangeForTextBox(m_renderer, m_selectableRange);
    if (selectionStart < selectionEnd)
        return { selectionStart, selectionEnd, MarkedText::Type::Selection };
    return { };
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintBackground()
{
    auto shouldPaintCompositionBackground = m_containsComposition && !m_useCustomUnderlines;
#if ENABLE(TEXT_SELECTION)
    auto hasSelectionWithNonCustomUnderline = m_haveSelection && !m_useCustomUnderlines;
#endif

    auto shouldPaintBackground = [&] {
#if ENABLE(TEXT_SELECTION)
        if (hasSelectionWithNonCustomUnderline)
            return true;
#endif
        if (shouldPaintCompositionBackground)
            return true;
        if (CheckedPtr markers = m_document.markersIfExists(); markers && markers->hasMarkers())
            return true;
        if (m_document.hasHighlight())
            return true;
        return false;
    };
    if (!shouldPaintBackground())
        return;

    if (shouldPaintCompositionBackground)
        paintCompositionBackground();

    Vector<MarkedText> markedTexts;
    markedTexts.appendVector(MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Background));
    markedTexts.appendVector(MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Background));

#if ENABLE(TEXT_SELECTION)
    if (hasSelectionWithNonCustomUnderline && !m_paintInfo.context().paintingDisabled()) {
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

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintCompositionForeground(const StyledMarkedText& markedText)
{
    auto& editor = m_renderer.frame().editor();

    if (!(editor.compositionUsesCustomHighlights() && m_containsComposition)) {
        paintForeground(markedText);
        return;
    }

    // The highlight ranges must be "packed" so that there is no non-empty interval between
    // any two adjacent highlight ranges. This is needed since otherwise, `paintForeground`
    // will not be called in those would-be non-empty intervals.
    auto highlights = editor.customCompositionHighlights();

    Vector<CompositionHighlight> highlightsWithForeground;
    highlightsWithForeground.append({ textBox().start(), highlights[0].startOffset, { }, { } });

    for (size_t i = 0; i < highlights.size(); ++i) {
        highlightsWithForeground.append(highlights[i]);
        if (i != highlights.size() - 1)
            highlightsWithForeground.append({ highlights[i].endOffset, highlights[i + 1].startOffset, { }, { } });
    }

    highlightsWithForeground.append({ highlights.last().endOffset, textBox().end(), { }, { } });

    auto& lineStyle = m_isFirstLine ? m_renderer.firstLineStyle() : m_renderer.style();

    for (auto& highlight : highlightsWithForeground) {
        auto style = StyledMarkedText::computeStyleForUnmarkedMarkedText(m_renderer, lineStyle, m_isFirstLine, m_paintInfo);

        if (highlight.endOffset <= textBox().start())
            continue;

        if (highlight.startOffset >= textBox().end())
            break;

        auto [clampedStart, clampedEnd] = m_selectableRange.clamp(highlight.startOffset, highlight.endOffset);

        if (highlight.foregroundColor)
            style.textStyles.fillColor = *highlight.foregroundColor;

        paintForeground({ MarkedText { clampedStart, clampedEnd, MarkedText::Type::Unmarked }, style });

        if (highlight.endOffset > textBox().end())
            break;
    }
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintForegroundAndDecorations()
{
    auto shouldPaintSelectionForeground = m_haveSelection && !m_useCustomUnderlines;
    auto hasTextDecoration = !m_style.textDecorationsInEffect().isEmpty();
    auto hasHighlightDecoration = m_document.hasHighlight() && !MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration).isEmpty();
    auto hasMismatchingContentDirection = m_renderer.containingBlock()->style().direction() != textBox().direction();
    auto hasBackwardTrunctation = m_selectableRange.truncation && hasMismatchingContentDirection;

    auto hasSpellingOrGrammarDecoration = [&] {
        auto markedTexts = MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration);

        auto hasSpellingError = markedTexts.containsIf([](auto&& markedText) {
            return markedText.type == MarkedText::Type::SpellingError;
        });

        if (hasSpellingError) {
            auto spellingErrorStyle = m_renderer.spellingErrorPseudoStyle();
            if (spellingErrorStyle)
                return !spellingErrorStyle->textDecorationsInEffect().isEmpty();
        }

        auto hasGrammarError = markedTexts.containsIf([](auto&& markedText) {
            return markedText.type == MarkedText::Type::GrammarError;
        });

        if (hasGrammarError) {
            auto grammarErrorStyle = m_renderer.grammarErrorPseudoStyle();
            if (grammarErrorStyle)
                return !grammarErrorStyle->textDecorationsInEffect().isEmpty();
        }

        return false;
    };

    auto hasDecoration = hasTextDecoration || hasHighlightDecoration || hasSpellingOrGrammarDecoration();

    auto contentMayNeedStyledMarkedText = [&] {
        if (hasDecoration)
            return true;
        if (shouldPaintSelectionForeground)
            return true;
        if (CheckedPtr markers = m_document.markersIfExists(); markers && markers->hasMarkers())
            return true;
        if (m_document.hasHighlight())
            return true;
        return false;
    };
    auto startPosition = [&] {
        return !hasBackwardTrunctation ? m_selectableRange.clamp(textBox().start()) : textBox().length() - *m_selectableRange.truncation;
    };
    auto endPosition = [&] {
        return !hasBackwardTrunctation ? m_selectableRange.clamp(textBox().end()) : textBox().length();
    };
    if (!contentMayNeedStyledMarkedText()) {
        auto& lineStyle = m_isFirstLine ? m_renderer.firstLineStyle() : m_renderer.style();
        auto markedText = MarkedText { startPosition(), endPosition(), MarkedText::Type::Unmarked };
        auto styledMarkedText = StyledMarkedText { markedText, StyledMarkedText::computeStyleForUnmarkedMarkedText(m_renderer, lineStyle, m_isFirstLine, m_paintInfo) };
        paintCompositionForeground(styledMarkedText);
        return;
    }

    Vector<MarkedText> markedTexts;
    if (m_paintInfo.phase != PaintPhase::Selection) {
        // The marked texts for the gaps between document markers and selection are implicitly created by subdividing the entire line.
        markedTexts.append({ startPosition(), endPosition(), MarkedText::Type::Unmarked });

        if (!m_isPrinting) {
            markedTexts.appendVector(MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Foreground));
            markedTexts.appendVector(MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Foreground));

            bool shouldPaintDraggedContent = !(m_paintInfo.paintBehavior.contains(PaintBehavior::ExcludeSelection));
            if (shouldPaintDraggedContent) {
                auto markedTextsForDraggedContent = MarkedText::collectForDraggedAndTransparentContent(DocumentMarker::Type::DraggedContent, m_renderer, m_selectableRange);
                if (!markedTextsForDraggedContent.isEmpty()) {
                    shouldPaintSelectionForeground = false;
                    markedTexts.appendVector(WTFMove(markedTextsForDraggedContent));
                }
            }
            auto markedTextsForTransparentContent = MarkedText::collectForDraggedAndTransparentContent(DocumentMarker::Type::TransparentContent, m_renderer, m_selectableRange);
            if (!markedTextsForTransparentContent.isEmpty())
                markedTexts.appendVector(WTFMove(markedTextsForTransparentContent));
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
            return markedText.type == MarkedText::Type::Selection;
        });
    }

    if (hasDecoration && m_paintInfo.phase != PaintPhase::Selection) {
        unsigned length = m_selectableRange.truncation.value_or(m_paintTextRun.length());
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
                auto snappedPaintRect = snapRectToDevicePixelsWithWritingDirection(LayoutRect { m_paintRect }, m_document.deviceScaleFactor(), m_paintTextRun.ltr());
                if (startOffset || endOffset != m_paintTextRun.length()) {
                    LayoutRect selectionRect = { m_paintRect.x(), m_paintRect.y(), m_paintRect.width(), m_paintRect.height() };
                    fontCascade().adjustSelectionRectForText(m_renderer.canUseSimplifiedTextMeasuring().value_or(false), m_paintTextRun, selectionRect, startOffset, endOffset);
                    snappedPaintRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_document.deviceScaleFactor(), m_paintTextRun.ltr());
                }
                auto decorationPainter = createDecorationPainter(markedText, textDecorationSelectionClipOutRect);
                paintBackgroundDecorations(decorationPainter, markedText, snappedPaintRect);
                paintCompositionForeground(markedText);
                paintForegroundDecorations(decorationPainter, markedText, snappedPaintRect);
            }
        }
    } else {
        // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
        auto coalescedStyledMarkedTexts = StyledMarkedText::coalesceAdjacentWithEqualForeground(styledMarkedTexts);

        if (coalescedStyledMarkedTexts.isEmpty())
            return;

        for (auto& markedText : coalescedStyledMarkedTexts)
            paintCompositionForeground(markedText);
    }
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintCompositionBackground()
{
    auto& editor = m_renderer.frame().editor();

    if (!editor.compositionUsesCustomHighlights()) {
        auto [clampedStart, clampedEnd] = m_selectableRange.clamp(editor.compositionStart(), editor.compositionEnd());

        paintBackground(clampedStart, clampedEnd, CompositionHighlight::defaultCompositionFillColor);
        return;
    }

    for (auto& highlight : editor.customCompositionHighlights()) {
        if (!highlight.backgroundColor)
            continue;

        if (highlight.endOffset <= textBox().start())
            continue;

        if (highlight.startOffset >= textBox().end())
            break;

        auto [clampedStart, clampedEnd] = m_selectableRange.clamp(highlight.startOffset, highlight.endOffset);

        paintBackground(clampedStart, clampedEnd, *highlight.backgroundColor, BackgroundStyle::Rounded);

        if (highlight.endOffset > textBox().end())
            break;
    }
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintBackground(const StyledMarkedText& markedText)
{
    paintBackground(markedText.startOffset, markedText.endOffset, markedText.style.backgroundColor, BackgroundStyle::Normal);
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintBackground(unsigned startOffset, unsigned endOffset, const Color& color, BackgroundStyle backgroundStyle)
{
    if (startOffset >= endOffset)
        return;

    GraphicsContext& context = m_paintInfo.context();
    GraphicsContextStateSaver stateSaver { context };
    updateGraphicsContext(context, TextPaintStyle { color }); // Don't draw text at all!

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own highlight.
    auto lineBox = makeIterator()->lineBox();
    auto selectionBottom = LineSelection::logicalBottom(*lineBox);
    auto selectionTop = LineSelection::logicalTopAdjustedForPrecedingBlock(*lineBox);
    // Use same y positioning and height as for selection, so that when the selection and this subrange are on
    // the same word there are no pieces sticking out.
    auto deltaY = LayoutUnit { m_style.isFlippedLinesWritingMode() ? selectionBottom - m_logicalRect.maxY() : m_logicalRect.y() - selectionTop };
    auto selectionHeight = LayoutUnit { std::max(0.f, selectionBottom - selectionTop) };
    auto selectionRect = LayoutRect { LayoutUnit(m_paintRect.x()), LayoutUnit(m_paintRect.y() - deltaY), LayoutUnit(m_logicalRect.width()), selectionHeight };
    auto adjustedSelectionRect = selectionRect;
    fontCascade().adjustSelectionRectForText(m_renderer.canUseSimplifiedTextMeasuring().value_or(false), m_paintTextRun, adjustedSelectionRect, startOffset, endOffset);
    if (m_paintTextRun.length() == endOffset - startOffset) {
        // FIXME: We should reconsider re-measuring the content when non-whitespace runs are joined together (see webkit.org/b/251318).
        auto visualRight = std::max(adjustedSelectionRect.maxX(), selectionRect.maxX());
        adjustedSelectionRect.shiftMaxXEdgeTo(visualRight);
    }

    // FIXME: Support painting combined text. See <https://bugs.webkit.org/show_bug.cgi?id=180993>.
    auto backgroundRect = snapRectToDevicePixels(adjustedSelectionRect, m_document.deviceScaleFactor());
    if (backgroundStyle == BackgroundStyle::Rounded) {
        backgroundRect.expand(-1, -1);
        backgroundRect.move(0.5, 0.5);
        context.fillRoundedRect(FloatRoundedRect { backgroundRect, FloatRoundedRect::Radii { 2 } }, color);
        return;
    }

    context.fillRect(backgroundRect, color);
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintForeground(const StyledMarkedText& markedText)
{
    if (markedText.startOffset >= markedText.endOffset)
        return;

    GraphicsContext& context = m_paintInfo.context();
    const FontCascade& font = fontCascade();

    float emphasisMarkOffset = 0;
    const AtomString& emphasisMark = m_emphasisMarkExistsAndIsAbove ? m_style.textEmphasisMarkString() : nullAtom();
    if (!emphasisMark.isEmpty())
        emphasisMarkOffset = *m_emphasisMarkExistsAndIsAbove ? -font.metricsOfPrimaryFont().intAscent() - font.emphasisMarkDescent(emphasisMark) : font.metricsOfPrimaryFont().intDescent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context, font, m_style };
    textPainter.setStyle(markedText.style.textStyles);
    textPainter.setIsHorizontal(textBox().isHorizontal());
    if (markedText.style.textShadow) {
        textPainter.setShadow(&markedText.style.textShadow.value());
        if (m_style.hasAppleColorFilter())
            textPainter.setShadowColorFilter(&m_style.appleColorFilter());
    }
    textPainter.setEmphasisMark(emphasisMark, emphasisMarkOffset, m_isCombinedText ? &downcast<RenderCombineText>(m_renderer) : nullptr);
    if (auto* debugShadow = debugTextShadow())
        textPainter.setShadow(debugShadow);

    bool isTransparentMarkedText = markedText.type == MarkedText::Type::DraggedContent || markedText.type == MarkedText::Type::TransparentContent;
    GraphicsContextStateSaver stateSaver(context, markedText.style.textStyles.strokeWidth > 0 || isTransparentMarkedText);
    if (isTransparentMarkedText)
        context.setAlpha(markedText.style.alpha);
    updateGraphicsContext(context, markedText.style.textStyles);

    if constexpr (std::is_same_v<TextBoxPath, InlineIterator::BoxLegacyPath>)
        textPainter.setGlyphDisplayListIfNeeded(downcast<LegacyInlineTextBox>(*textBox().legacyInlineBox()), m_paintInfo, m_paintTextRun);
    else
        textPainter.setGlyphDisplayListIfNeeded(textBox().box(), m_paintInfo, m_paintTextRun);

    // TextPainter wants the box rectangle and text origin of the entire line box.
    textPainter.paintRange(m_paintTextRun, m_paintRect, textOriginFromPaintRect(m_paintRect), markedText.startOffset, markedText.endOffset);
}

template<typename TextBoxPath>
TextDecorationPainter TextBoxPainter<TextBoxPath>::createDecorationPainter(const StyledMarkedText& markedText, const FloatRect& clipOutRect)
{
    GraphicsContext& context = m_paintInfo.context();

    updateGraphicsContext(context, markedText.style.textStyles);

    // Note that if the text is truncated, we let the thing being painted in the truncation
    // draw its own decoration.
    GraphicsContextStateSaver stateSaver { context, false };
    bool isTransparentContent = markedText.type == MarkedText::Type::DraggedContent || markedText.type == MarkedText::Type::TransparentContent;
    if (isTransparentContent || !clipOutRect.isEmpty()) {
        stateSaver.save();
        if (isTransparentContent)
            context.setAlpha(markedText.style.alpha);
        if (!clipOutRect.isEmpty())
            context.clipOut(clipOutRect);
    }

    // Create painter
    auto* shadow = markedText.style.textShadow ? &markedText.style.textShadow.value() : nullptr;
    auto* colorFilter = markedText.style.textShadow && m_style.hasAppleColorFilter() ? &m_style.appleColorFilter() : nullptr;
    return { context, fontCascade(), shadow, colorFilter, m_document.printing(), m_renderer.isHorizontalWritingMode() };
}

static inline float computedTextDecorationThickness(const RenderStyle& styleToUse, float deviceScaleFactor)
{
    return ceilToDevicePixel(styleToUse.textDecorationThickness().resolve(styleToUse.computedFontSize(), styleToUse.metricsOfPrimaryFont()), deviceScaleFactor);
}

static inline float computedAutoTextDecorationThickness(const RenderStyle& styleToUse, float deviceScaleFactor)
{
    return ceilToDevicePixel(TextDecorationThickness::createWithAuto().resolve(styleToUse.computedFontSize(), styleToUse.metricsOfPrimaryFont()), deviceScaleFactor);
}

static inline float computedLinethroughCenter(const RenderStyle& styleToUse, float textDecorationThickness, float autoTextDecorationThickness)
{
    auto center = 2 * styleToUse.metricsOfPrimaryFont().ascent() / 3 + autoTextDecorationThickness / 2;
    return center - textDecorationThickness / 2;
}

static inline OptionSet<TextDecorationLine> computedTextDecorationType(const RenderStyle& style, const TextDecorationPainter::Styles& textDecorationStyles)
{
    auto textDecorations = style.textDecorationsInEffect();
    textDecorations.add(TextDecorationPainter::textDecorationsInEffectForStyle(textDecorationStyles));
    return textDecorations;
}

static inline bool isDecoratingBoxForBackground(const InlineIterator::InlineBox& inlineBox, const RenderStyle& styleToUse)
{
    if (auto* element = inlineBox.renderer().element(); element && (is<HTMLAnchorElement>(*element) || element->hasTagName(HTMLNames::fontTag))) {
        // <font> and <a> are always considered decorating boxes.
        return true;
    }
    return styleToUse.textDecorationLine().containsAny({ TextDecorationLine::Underline, TextDecorationLine::Overline })
        || (inlineBox.isRootInlineBox() && styleToUse.textDecorationsInEffect().containsAny({ TextDecorationLine::Underline, TextDecorationLine::Overline }));
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::collectDecoratingBoxesForTextBox(DecoratingBoxList& decoratingBoxList, const InlineIterator::TextBoxIterator& textBox, FloatPoint textBoxLocation, const TextDecorationPainter::Styles& overrideDecorationStyle)
{
    auto ancestorInlineBox = textBox->parentInlineBox();
    if (!ancestorInlineBox) {
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME: Vertical writing mode needs some coordinate space transformation for parent inline boxes as we rotate the content with m_paintRect (see ::paint)
    if (ancestorInlineBox->isRootInlineBox() || !textBox->isHorizontal()) {
        decoratingBoxList.append({
            ancestorInlineBox,
            m_isFirstLine ? m_renderer.firstLineStyle() : m_renderer.style(),
            overrideDecorationStyle,
            textBoxLocation
        });
        return;
    }

    enum UseOverriderDecorationStyle : bool { No, Yes };
    auto appendIfIsDecoratingBoxForBackground = [&] (auto& inlineBox, auto useOverriderDecorationStyle) {
        auto& style = m_isFirstLine ? inlineBox->renderer().firstLineStyle() : inlineBox->renderer().style();

        auto computedDecorationStyle = [&] {
            return TextDecorationPainter::stylesForRenderer(inlineBox->renderer(), style.textDecorationsInEffect(), m_isFirstLine);
        };
        if (!isDecoratingBoxForBackground(*inlineBox, style)) {
            // Some cases even non-decoration boxes may have some decoration pieces coming from the marked text (e.g. highlight).
            if (useOverriderDecorationStyle == UseOverriderDecorationStyle::No || overrideDecorationStyle == computedDecorationStyle())
                return;
        }

        auto borderAndPaddingBefore = !inlineBox->isRootInlineBox() ? inlineBox->renderer().borderAndPaddingBefore() : LayoutUnit(0_lu);
        decoratingBoxList.append({
            inlineBox,
            style,
            useOverriderDecorationStyle == UseOverriderDecorationStyle::Yes ? overrideDecorationStyle : computedDecorationStyle(),
            { textBoxLocation.x(), m_paintOffset.y() + inlineBox->logicalTop() + borderAndPaddingBefore }
        });
    };

    // FIXME: Figure out if the decoration styles coming from the styled marked text should be used only on the closest inline box (direct parent).
    appendIfIsDecoratingBoxForBackground(ancestorInlineBox, UseOverriderDecorationStyle::Yes);
    while (!ancestorInlineBox->isRootInlineBox()) {
        ancestorInlineBox = ancestorInlineBox->parentInlineBox();
        if (!ancestorInlineBox) {
            ASSERT_NOT_REACHED();
            break;
        }
        appendIfIsDecoratingBoxForBackground(ancestorInlineBox, UseOverriderDecorationStyle::No);
    }
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintBackgroundDecorations(TextDecorationPainter& decorationPainter, const StyledMarkedText& markedText, const FloatRect& textBoxPaintRect)
{
    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Clockwise));

    auto textRun = m_paintTextRun.subRun(markedText.startOffset, markedText.endOffset - markedText.startOffset);

    auto textBox = makeIterator();
    auto decoratingBoxList = DecoratingBoxList { };
    collectDecoratingBoxesForTextBox(decoratingBoxList, textBox, textBoxPaintRect.location(), markedText.style.textDecorationStyles);

    for (auto& decoratingBox : makeReversedRange(decoratingBoxList)) {
        auto computedTextDecorationType = WebCore::computedTextDecorationType(decoratingBox.style, decoratingBox.textDecorationStyles);
        auto computedBackgroundDecorationGeometry = [&] {
            auto textDecorationThickness = computedTextDecorationThickness(decoratingBox.style, m_document.deviceScaleFactor());
            auto underlineOffset = [&] {
                if (!computedTextDecorationType.contains(TextDecorationLine::Underline))
                    return 0.f;
                auto baseOffset = underlineOffsetForTextBoxPainting(*decoratingBox.inlineBox, decoratingBox.style);
                auto wavyOffset = decoratingBox.textDecorationStyles.underline.decorationStyle == TextDecorationStyle::Wavy ? wavyOffsetFromDecoration() : 0.f;
                return baseOffset + wavyOffset;
            };
            auto autoTextDecorationThickness = computedAutoTextDecorationThickness(decoratingBox.style, m_document.deviceScaleFactor());
            auto overlineOffset = [&] {
                if (!computedTextDecorationType.contains(TextDecorationLine::Overline))
                    return 0.f;
                auto baseOffset = overlineOffsetForTextBoxPainting(*decoratingBox.inlineBox, decoratingBox.style);
                baseOffset += (autoTextDecorationThickness - textDecorationThickness);
                auto wavyOffset = decoratingBox.textDecorationStyles.overline.decorationStyle == TextDecorationStyle::Wavy ? wavyOffsetFromDecoration() : 0.f;
                return baseOffset - wavyOffset;
            };

            return TextDecorationPainter::BackgroundDecorationGeometry {
                textOriginFromPaintRect(textBoxPaintRect),
                roundPointToDevicePixels(LayoutPoint { decoratingBox.location }, m_document.deviceScaleFactor(), m_paintTextRun.ltr()),
                textBoxPaintRect.width(),
                textDecorationThickness,
                underlineOffset(),
                overlineOffset(),
                computedLinethroughCenter(decoratingBox.style, textDecorationThickness, autoTextDecorationThickness),
                decoratingBox.style.metricsOfPrimaryFont().intAscent() + 2.f,
                wavyStrokeParameters(decoratingBox.style.computedFontSize())
            };
        };

        decorationPainter.paintBackgroundDecorations(m_style, textRun, computedBackgroundDecorationGeometry(), computedTextDecorationType, decoratingBox.textDecorationStyles);
    }

    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Counterclockwise));
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintForegroundDecorations(TextDecorationPainter& decorationPainter, const StyledMarkedText& markedText, const FloatRect& textBoxPaintRect)
{
    auto& styleToUse = m_isFirstLine ? m_renderer.firstLineStyle() : m_renderer.style();
    auto computedTextDecorationType = [&] {
        auto textDecorations = styleToUse.textDecorationsInEffect();
        textDecorations.add(TextDecorationPainter::textDecorationsInEffectForStyle(markedText.style.textDecorationStyles));
        return textDecorations;
    }();

    if (!computedTextDecorationType.contains(TextDecorationLine::LineThrough))
        return;

    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Clockwise));

    auto deviceScaleFactor = m_document.deviceScaleFactor();
    auto textDecorationThickness = computedTextDecorationThickness(styleToUse, deviceScaleFactor);
    auto linethroughCenter = computedLinethroughCenter(styleToUse, textDecorationThickness, computedAutoTextDecorationThickness(styleToUse, deviceScaleFactor));
    decorationPainter.paintForegroundDecorations({ textBoxPaintRect.location()
        , textBoxPaintRect.width()
        , textDecorationThickness
        , linethroughCenter
        , wavyStrokeParameters(styleToUse.computedFontSize()) }, markedText.style.textDecorationStyles);

    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, RotationDirection::Counterclockwise));
}

static FloatRoundedRect::Radii radiiForUnderline(const CompositionUnderline& underline, unsigned markedTextStartOffset, unsigned markedTextEndOffset)
{
    auto radii = FloatRoundedRect::Radii { 0 };

#if HAVE(REDESIGNED_TEXT_CURSOR)
    if (!redesignedTextCursorEnabled())
        return radii;

    if (underline.startOffset >= markedTextStartOffset) {
        radii.setTopLeft({ 1, 1 });
        radii.setBottomLeft({ 1, 1 });
    }

    if (underline.endOffset <= markedTextEndOffset) {
        radii.setTopRight({ 1, 1 });
        radii.setBottomRight({ 1, 1 });
    }
#else
    UNUSED_PARAM(underline);
    UNUSED_PARAM(markedTextStartOffset);
    UNUSED_PARAM(markedTextEndOffset);
#endif

    return radii;
}

#if HAVE(REDESIGNED_TEXT_CURSOR)
enum class TrimSide : bool {
    Left,
    Right,
};

static FloatRoundedRect::Radii trimRadii(const FloatRoundedRect::Radii& radii, TrimSide trimSide)
{
    switch (trimSide) {
    case TrimSide::Left:
        return { { }, radii.topRight(), { }, radii.bottomRight() };
    case TrimSide::Right:
        return { radii.topLeft(), { }, radii.bottomLeft(), { } };
    }
}

enum class SnapDirection : uint8_t {
    Left,
    Right,
    Both,
};

static FloatRect snapRectToDevicePixelsInDirection(const FloatRect& rect, float deviceScaleFactor, SnapDirection snapDirection)
{
    const auto layoutRect = LayoutRect { rect };
    switch (snapDirection) {
    case SnapDirection::Left:
        return snapRectToDevicePixelsWithWritingDirection(layoutRect, deviceScaleFactor, true);
    case SnapDirection::Right:
        return snapRectToDevicePixelsWithWritingDirection(layoutRect, deviceScaleFactor, false);
    case SnapDirection::Both:
        auto snappedRectLeft = snapRectToDevicePixelsWithWritingDirection(layoutRect, deviceScaleFactor, true);
        return snapRectToDevicePixelsWithWritingDirection(LayoutRect { snappedRectLeft }, deviceScaleFactor, false);
    }
}

enum class LayoutBoxLocation : uint8_t {
    OnlyBox,
    StartOfSequence,
    EndOfSequence,
    MiddleOfSequence,
    Unknown,
};

static LayoutBoxLocation layoutBoxSequenceLocation(const InlineIterator::BoxModernPath& textBox)
{
    auto isFirstForLayoutBox = textBox.box().isFirstForLayoutBox();
    auto isLastForLayoutBox = textBox.box().isLastForLayoutBox();
    if (isFirstForLayoutBox && isLastForLayoutBox)
        return LayoutBoxLocation::OnlyBox;
    if (isFirstForLayoutBox)
        return LayoutBoxLocation::StartOfSequence;
    if (isLastForLayoutBox)
        return LayoutBoxLocation::EndOfSequence;
    return LayoutBoxLocation::MiddleOfSequence;
}

static LayoutBoxLocation layoutBoxSequenceLocation(const InlineIterator::BoxLegacyPath&)
{
    return LayoutBoxLocation::Unknown;
}
#endif

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::fillCompositionUnderline(float start, float width, const CompositionUnderline& underline, const FloatRoundedRect::Radii& radii, bool hasLiveConversion) const
{
#if HAVE(REDESIGNED_TEXT_CURSOR)
    if (!redesignedTextCursorEnabled())
#endif
    {
        // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
        // All other marked text underlines are 1px thick.
        // If there's not enough space the underline will touch or overlap characters.
        int lineThickness = 1;
        int baseline = m_style.metricsOfPrimaryFont().intAscent();
        if (underline.thick && m_logicalRect.height() - baseline >= 2)
            lineThickness = 2;

        // We need to have some space between underlines of subsequent clauses, because some input methods do not use different underline styles for those.
        // We make each line shorter, which has a harmless side effect of shortening the first and last clauses, too.
        start += 1;
        width -= 2;

        auto& style = m_renderer.style();
        auto underlineColor = underline.compositionUnderlineColor == CompositionUnderlineColor::TextColor ? style.visitedDependentColorWithColorFilter(CSSPropertyWebkitTextFillColor) : style.colorByApplyingColorFilter(underline.color);

        auto& context = m_paintInfo.context();
        context.setStrokeColor(underlineColor);
        context.setStrokeThickness(lineThickness);
        context.drawLineForText(FloatRect(m_paintRect.x() + start, m_paintRect.y() + m_logicalRect.height() - lineThickness, width, lineThickness), m_isPrinting);
        return;
    }

#if HAVE(REDESIGNED_TEXT_CURSOR)
    if (!underline.color.isVisible())
        return;

    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = m_style.metricsOfPrimaryFont().intAscent();
    if (m_logicalRect.height() - baseline >= 2)
        lineThickness = 2;

    auto underlineColor = [this] {
#if PLATFORM(MAC)
        auto cssColorValue = CSSValueAppleSystemControlAccent;
#else
        auto cssColorValue = CSSValueAppleSystemBlue;
#endif
        auto styleColorOptions = m_renderer.styleColorOptions();
        return RenderTheme::singleton().systemColor(cssColorValue, styleColorOptions | StyleColorOptions::UseSystemAppearance);
    }();

    if (!underline.thick && hasLiveConversion)
        underlineColor = underlineColor.colorWithAlpha(0.35);

    auto& context = m_paintInfo.context();
    context.setStrokeColor(underlineColor);
    context.setStrokeThickness(lineThickness);

    auto rect = FloatRect(m_paintRect.x() + start, m_paintRect.y() + m_logicalRect.height() - lineThickness, width, lineThickness);

    if (radii.isZero()) {
        context.drawLineForText(rect, m_isPrinting);
        return;
    }

    // We cannot directly draw rounded edges for every rect, since a single textbox path may be split up over multiple rects.
    // Drawing rounded edges unconditionally could then produce broken underlines between continuous rects.
    // As a mitigation, we consult the textbox path to understand the current rect's position in the textbox path.
    // If we're the only box in the path, then we fallback to unconditionally drawing rounded edges.
    // If not, we flatten out the right, left, or both edges depending on whether we're at the start, end, or middle of a path, respectively.

    auto deviceScaleFactor = m_document.deviceScaleFactor();

    switch (layoutBoxSequenceLocation(m_textBox)) {
    case LayoutBoxLocation::Unknown:
    case LayoutBoxLocation::OnlyBox: {
        context.fillRoundedRect(FloatRoundedRect { rect, radii }, underlineColor);
        return;
    }
    case LayoutBoxLocation::StartOfSequence: {
        auto snappedRectRight = snapRectToDevicePixelsInDirection(rect, deviceScaleFactor, SnapDirection::Right);
        context.fillRoundedRect(FloatRoundedRect { snappedRectRight, trimRadii(radii, TrimSide::Right) }, underlineColor);
        return;
    }
    case LayoutBoxLocation::EndOfSequence: {
        auto snappedRectLeft = snapRectToDevicePixelsInDirection(rect, deviceScaleFactor, SnapDirection::Left);
        context.fillRoundedRect(FloatRoundedRect { snappedRectLeft, trimRadii(radii, TrimSide::Left) }, underlineColor);
        return;
    }
    case LayoutBoxLocation::MiddleOfSequence: {
        auto snappedRectBoth = snapRectToDevicePixelsInDirection(rect, deviceScaleFactor, SnapDirection::Both);
        context.fillRect(snappedRectBoth, underlineColor);
        return;
    }
    }
    ASSERT_NOT_REACHED("Unexpected LayoutBoxLocation value, underline not drawn");
#else
    UNUSED_PARAM(radii);
    UNUSED_PARAM(hasLiveConversion);
#endif
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintCompositionUnderlines()
{        
    auto& underlines = m_renderer.frame().editor().customCompositionUnderlines();
    auto underlineCount = underlines.size();

    if (!underlineCount)
        return;

    auto hasLiveConversion = false;

    auto markedTextStartOffset = underlines[0].startOffset;
    auto markedTextEndOffset = underlines[0].endOffset;

    for (const auto& underline : underlines) {
        if (underline.thick)
            hasLiveConversion = true;

        if (underline.startOffset < markedTextStartOffset)
            markedTextStartOffset = underline.startOffset;

        if (underline.endOffset > markedTextEndOffset)
            markedTextEndOffset = underline.endOffset;
    }

    for (size_t i = 0; i < underlineCount; ++i) {
        auto& underline = underlines[i];
        if (underline.endOffset <= textBox().start()) {
            // Underline is completely before this run. This might be an underline that sits
            // before the first run we draw, or underlines that were within runs we skipped
            // due to truncation.
            continue;
        }

        if (underline.startOffset >= textBox().end())
            break; // Underline is completely after this run, bail. A later run will paint it.

        auto underlineRadii = radiiForUnderline(underline, markedTextStartOffset, markedTextEndOffset);

        // Underline intersects this run. Paint it.
        paintCompositionUnderline(underline, underlineRadii, hasLiveConversion);

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

template<typename TextBoxPath>
float TextBoxPainter<TextBoxPath>::textPosition()
{
    // When computing the width of a text run, RenderBlock::computeInlineDirectionPositionsForLine() doesn't include the actual offset
    // from the containing block edge in its measurement. textPosition() should be consistent so the text are rendered in the same width.
    if (!m_logicalRect.x())
        return 0;
    return m_logicalRect.x() - makeIterator()->lineBox()->contentLogicalLeft();
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintCompositionUnderline(const CompositionUnderline& underline, const FloatRoundedRect::Radii& radii, bool hasLiveConversion)
{
    float start = 0; // start of line to draw, relative to tx
    float width = m_logicalRect.width(); // how much line to draw
    bool useWholeWidth = true;
    unsigned paintStart = textBox().start();
    unsigned paintEnd = textBox().end();
    if (paintStart <= underline.startOffset) {
        paintStart = underline.startOffset;
        useWholeWidth = false;
        start = m_renderer.width(textBox().start(), paintStart - textBox().start(), textPosition(), m_isFirstLine);
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
        width = m_renderer.width(paintStart, paintEnd - paintStart, textPosition() + start, m_isFirstLine);
        mirrorRTLSegment(m_logicalRect.width(), textBox().direction(), start, width);
    }

    fillCompositionUnderline(start, width, underline, radii, hasLiveConversion);
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintPlatformDocumentMarkers()
{
    auto markedTexts = MarkedText::collectForDocumentMarkers(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration);

    auto spellingErrorStyle = m_renderer.spellingErrorPseudoStyle();
    if (spellingErrorStyle && !spellingErrorStyle->textDecorationsInEffect().isEmpty()) {
        markedTexts.removeAllMatching([] (auto&& markedText) {
            return markedText.type == MarkedText::Type::SpellingError;
        });
    }

    auto grammarErrorStyle = m_renderer.grammarErrorPseudoStyle();
    if (grammarErrorStyle && !grammarErrorStyle->textDecorationsInEffect().isEmpty()) {
        markedTexts.removeAllMatching([] (auto&& markedText) {
            return markedText.type == MarkedText::Type::GrammarError;
        });
    }

    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        paintPlatformDocumentMarker(markedText);
}

FloatRect LegacyTextBoxPainter::calculateUnionOfAllDocumentMarkerBounds(const LegacyInlineTextBox& textBox)
{
    // This must match paintPlatformDocumentMarkers().
    FloatRect result;
    auto markedTexts = MarkedText::collectForDocumentMarkers(textBox.renderer(), textBox.selectableRange(), MarkedText::PaintPhase::Decoration);
    for (auto& markedText : MarkedText::subdivide(markedTexts, MarkedText::OverlapStrategy::Frontmost))
        result = unionRect(result, calculateDocumentMarkerBounds(InlineIterator::textBoxFor(&textBox), markedText));
    return result;
}

#if ENABLE(WRITING_TOOLS)

#if USE(APPLE_INTERNAL_SDK)
#import <WebKitAdditions/TextBoxPainterAdditions.cpp>
#else
static void drawUnifiedTextReplacementUnderline(GraphicsContext&, const FloatRect&, IntSize) { }
#endif

#endif // ENABLE(WRITING_TOOLS)

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintPlatformDocumentMarker(const MarkedText& markedText)
{
    // Never print document markers (rdar://5327887)
    if (m_document.printing())
        return;

    auto bounds = calculateDocumentMarkerBounds(makeIterator(), markedText);
    bounds.moveBy(m_paintRect.location());

#if ENABLE(WRITING_TOOLS)
    if (markedText.type == MarkedText::Type::WritingToolsTextSuggestion) {
        drawUnifiedTextReplacementUnderline(m_paintInfo.context(), bounds,  m_renderer.frame().view()->size());
        return;
    }
#endif

    auto lineStyleMode = [&] {
        switch (markedText.type) {
        case MarkedText::Type::SpellingError:
            return DocumentMarkerLineStyleMode::Spelling;
        case MarkedText::Type::GrammarError:
            return DocumentMarkerLineStyleMode::Grammar;
        case MarkedText::Type::Correction:
            return DocumentMarkerLineStyleMode::AutocorrectionReplacement;
        case MarkedText::Type::DictationAlternatives:
            return DocumentMarkerLineStyleMode::DictationAlternatives;
#if PLATFORM(IOS_FAMILY)
        case MarkedText::Type::DictationPhraseWithAlternatives:
            // FIXME: Rename DocumentMarkerLineStyle::TextCheckingDictationPhraseWithAlternatives and remove the PLATFORM(IOS_FAMILY)-guard.
            return DocumentMarkerLineStyleMode::TextCheckingDictationPhraseWithAlternatives;
#endif
        default:
            ASSERT_NOT_REACHED();
            return DocumentMarkerLineStyleMode::Spelling;
        }
    }();

    auto lineStyleColor = RenderTheme::singleton().documentMarkerLineColor(m_renderer, lineStyleMode);
    if (auto* marker = markedText.marker)
        lineStyleColor = lineStyleColor.colorWithAlphaMultipliedBy(marker->opacity());

    m_paintInfo.context().drawDotsForDocumentMarker(bounds, { lineStyleMode, lineStyleColor });
}

template<typename TextBoxPath>
FloatRect TextBoxPainter<TextBoxPath>::computePaintRect(const LayoutPoint& paintOffset)
{
    FloatPoint localPaintOffset(paintOffset);

    localPaintOffset.move(0, m_style.isHorizontalWritingMode() ? 0 : -m_logicalRect.height());
    auto visualRect = textBox().visualRectIgnoringBlockDirection();
    textBox().formattingContextRoot().flipForWritingMode(visualRect);

    auto boxOrigin = visualRect.location();
    boxOrigin.moveBy(localPaintOffset);
    return { boxOrigin, FloatSize(m_logicalRect.width(), m_logicalRect.height()) };
}

FloatRect calculateDocumentMarkerBounds(const InlineIterator::TextBoxIterator& textBox, const MarkedText& markedText)
{
    auto& font = textBox->fontCascade();
    auto [y, height] = DocumentMarkerController::markerYPositionAndHeightForFont(font);

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (markedText.startOffset || markedText.endOffset != textBox->selectableRange().clamp(textBox->end())) {
        auto run = textBox->textRun();
        auto selectionRect = LayoutRect { 0_lu, y, 0_lu, height };
        font.adjustSelectionRectForText(textBox->renderer().canUseSimplifiedTextMeasuring().value_or(false), run, selectionRect, markedText.startOffset, markedText.endOffset);
        return selectionRect;
    }

    return FloatRect(0, y, textBox->logicalWidth(), height);
}

template<typename TextBoxPath>
bool TextBoxPainter<TextBoxPath>::computeHaveSelection() const
{
    if (m_isPrinting || m_paintInfo.phase == PaintPhase::TextClip)
        return false;

    return m_renderer.view().selection().highlightStateForTextBox(m_renderer, m_selectableRange) != RenderObject::HighlightState::None;
}

template<typename TextBoxPath>
const FontCascade& TextBoxPainter<TextBoxPath>::fontCascade() const
{
    if (m_isCombinedText)
        return downcast<RenderCombineText>(m_renderer).textCombineFont();

    return m_textBox.style().fontCascade();
}

template<typename TextBoxPath>
FloatPoint TextBoxPainter<TextBoxPath>::textOriginFromPaintRect(const FloatRect& paintRect) const
{
    FloatPoint textOrigin { paintRect.x(), paintRect.y() + fontCascade().metricsOfPrimaryFont().intAscent() };
    if (m_isCombinedText) {
        if (auto newOrigin = downcast<RenderCombineText>(m_renderer).computeTextOrigin(paintRect))
            textOrigin = newOrigin.value();
    }
    if (textBox().isHorizontal())
        textOrigin.setY(roundToDevicePixel(LayoutUnit { textOrigin.y() }, m_renderer.document().deviceScaleFactor()));
    else
        textOrigin.setX(roundToDevicePixel(LayoutUnit { textOrigin.x() }, m_renderer.document().deviceScaleFactor()));
    return textOrigin;
}

template<typename TextBoxPath>
const ShadowData* TextBoxPainter<TextBoxPath>::debugTextShadow() const
{
    if (!m_renderer.settings().legacyLineLayoutVisualCoverageEnabled())
        return nullptr;
    if constexpr (std::is_same_v<TextBoxPath, InlineIterator::BoxModernPath>)
        return nullptr;

    static NeverDestroyed<ShadowData> debugTextShadow(LengthPoint(Length(LengthType::Fixed), Length(LengthType::Fixed)), Length(10, LengthType::Fixed), Length(20, LengthType::Fixed), ShadowStyle::Normal, true, SRGBA<uint8_t> { 150, 0, 0, 190 });
    return &debugTextShadow.get();
}

template class TextBoxPainter<InlineIterator::BoxModernPath>;
template class TextBoxPainter<InlineIterator::BoxLegacyPath>;

}
