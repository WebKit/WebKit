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
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "EventRegion.h"
#include "GraphicsContext.h"
#include "HTMLAnchorElement.h"
#include "InlineIteratorLineBox.h"
#include "InlineTextBoxStyle.h"
#include "LegacyInlineTextBox.h"
#include "LineSelection.h"
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
    , m_isCombinedText(is<RenderCombineText>(m_renderer) && downcast<RenderCombineText>(m_renderer).isCombined())
    , m_isPrinting(m_document.printing())
    , m_haveSelection(computeHaveSelection())
    , m_containsComposition(m_renderer.textNode() && m_renderer.frame().editor().compositionNode() == m_renderer.textNode())
    , m_useCustomUnderlines(m_containsComposition && m_renderer.frame().editor().compositionUsesCustomUnderlines())
    , m_emphasisMarkExistsAndIsAbove(RenderText::emphasisMarkExistsAndIsAbove(m_renderer, m_style))
{
    ASSERT(paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::Selection || paintInfo.phase == PaintPhase::TextClip || paintInfo.phase == PaintPhase::EventRegion);
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
            m_paintInfo.eventRegionContext->unite(enclosingIntRect(m_paintRect), const_cast<RenderText&>(m_renderer), m_style);
        return;
    }

    bool shouldRotate = !textBox().isHorizontal() && !m_isCombinedText;
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

template<typename TextBoxPath>
MarkedText TextBoxPainter<TextBoxPath>::createMarkedTextFromSelectionInBox()
{
    auto [selectionStart, selectionEnd] = m_renderer.view().selection().rangeForTextBox(m_renderer, m_selectableRange);
    if (selectionStart < selectionEnd)
        return { selectionStart, selectionEnd, MarkedText::Selection };
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
        if (m_document.markers().hasMarkers())
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
void TextBoxPainter<TextBoxPath>::paintForegroundAndDecorations()
{
    auto shouldPaintSelectionForeground = m_haveSelection && !m_useCustomUnderlines;
    auto hasTextDecoration = !m_style.textDecorationsInEffect().isEmpty();
    auto hasHighlightDecoration = m_document.hasHighlight() && !MarkedText::collectForHighlights(m_renderer, m_selectableRange, MarkedText::PaintPhase::Decoration).isEmpty();
    auto hasDecoration = hasTextDecoration || hasHighlightDecoration;

    auto contentMayNeedStyledMarkedText = [&] {
        if (hasDecoration)
            return true;
        if (shouldPaintSelectionForeground)
            return true;
        if (m_document.markers().hasMarkers())
            return true;
        if (m_document.hasHighlight())
            return true;
        return false;
    };
    if (!contentMayNeedStyledMarkedText()) {
        auto& lineStyle = m_isFirstLine ? m_renderer.firstLineStyle() : m_renderer.style();
        paintForeground({ MarkedText { m_selectableRange.clamp(textBox().start()), m_selectableRange.clamp(textBox().end()), MarkedText::Unmarked },
            StyledMarkedText::computeStyleForUnmarkedMarkedText(m_renderer, lineStyle, m_isFirstLine, m_paintInfo) });
        return;
    }

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
                    fontCascade().adjustSelectionRectForText(m_paintTextRun, selectionRect, startOffset, endOffset);
                    snappedPaintRect = snapRectToDevicePixelsWithWritingDirection(selectionRect, m_document.deviceScaleFactor(), m_paintTextRun.ltr());
                }
                auto decorationPainter = createDecorationPainter(markedText, textDecorationSelectionClipOutRect);
                paintBackgroundDecorations(decorationPainter, markedText, snappedPaintRect);
                paintForeground(markedText);
                paintForegroundDecorations(decorationPainter, markedText, snappedPaintRect);
            }
        }
    } else {
        // Coalesce styles of adjacent marked texts to minimize the number of drawing commands.
        auto coalescedStyledMarkedTexts = StyledMarkedText::coalesceAdjacentWithEqualForeground(styledMarkedTexts);

        for (auto& markedText : coalescedStyledMarkedTexts)
            paintForeground(markedText);
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
        emphasisMarkOffset = *m_emphasisMarkExistsAndIsAbove ? -font.metricsOfPrimaryFont().ascent() - font.emphasisMarkDescent(emphasisMark) : font.metricsOfPrimaryFont().descent() + font.emphasisMarkAscent(emphasisMark);

    TextPainter textPainter { context, font };
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

    GraphicsContextStateSaver stateSaver(context, markedText.style.textStyles.strokeWidth > 0 || markedText.type == MarkedText::DraggedContent);
    if (markedText.type == MarkedText::DraggedContent)
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
    bool isDraggedContent = markedText.type == MarkedText::DraggedContent;
    if (isDraggedContent || !clipOutRect.isEmpty()) {
        stateSaver.save();
        if (isDraggedContent)
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
    auto center = 2 * styleToUse.metricsOfPrimaryFont().floatAscent() / 3 + autoTextDecorationThickness / 2;
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

    enum UseOverriderDecorationStyle : bool { Yes, No };
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
        appendIfIsDecoratingBoxForBackground(ancestorInlineBox, UseOverriderDecorationStyle::No);
    }
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintBackgroundDecorations(TextDecorationPainter& decorationPainter, const StyledMarkedText& markedText, const FloatRect& textBoxPaintRect)
{
    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Clockwise));

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
                return autoTextDecorationThickness - textDecorationThickness - (decoratingBox.textDecorationStyles.overline.decorationStyle == TextDecorationStyle::Wavy ? wavyOffsetFromDecoration() : 0.f);
            };

            return TextDecorationPainter::BackgroundDecorationGeometry {
                textOriginFromPaintRect(textBoxPaintRect),
                roundPointToDevicePixels(LayoutPoint { decoratingBox.location }, m_document.deviceScaleFactor(), m_paintTextRun.ltr()),
                textBoxPaintRect.width(),
                textDecorationThickness,
                underlineOffset(),
                overlineOffset(),
                computedLinethroughCenter(decoratingBox.style, textDecorationThickness, autoTextDecorationThickness),
                decoratingBox.style.metricsOfPrimaryFont().ascent() + 2.f,
                wavyStrokeParameters(decoratingBox.style.computedFontPixelSize())
            };
        };

        decorationPainter.paintBackgroundDecorations(textRun, computedBackgroundDecorationGeometry(), computedTextDecorationType, decoratingBox.textDecorationStyles);
    }

    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Counterclockwise));
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
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Clockwise));

    auto deviceScaleFactor = m_document.deviceScaleFactor();
    auto textDecorationThickness = computedTextDecorationThickness(styleToUse, deviceScaleFactor);
    auto linethroughCenter = computedLinethroughCenter(styleToUse, textDecorationThickness, computedAutoTextDecorationThickness(styleToUse, deviceScaleFactor));
    decorationPainter.paintForegroundDecorations({ textBoxPaintRect.location()
        , textBoxPaintRect.width()
        , textDecorationThickness
        , linethroughCenter
        , wavyStrokeParameters(styleToUse.computedFontPixelSize()) }, markedText.style.textDecorationStyles);

    if (m_isCombinedText)
        m_paintInfo.context().concatCTM(rotation(m_paintRect, Counterclockwise));
}

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/TextBoxPainterAdditions.cpp>
#else
static FloatRoundedRect::Radii radiiForUnderline(const CompositionUnderline&, unsigned, unsigned)
{
    return FloatRoundedRect::Radii { 0 };
}

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::fillCompositionUnderline(float start, float width, const CompositionUnderline& underline, const FloatRoundedRect::Radii&, bool) const
{
    // Thick marked text underlines are 2px thick as long as there is room for the 2px line under the baseline.
    // All other marked text underlines are 1px thick.
    // If there's not enough space the underline will touch or overlap characters.
    int lineThickness = 1;
    int baseline = m_style.metricsOfPrimaryFont().ascent();
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
}
#endif

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

template<typename TextBoxPath>
void TextBoxPainter<TextBoxPath>::paintPlatformDocumentMarker(const MarkedText& markedText)
{
    // Never print spelling/grammar markers (5327887)
    if (m_document.printing())
        return;

    auto bounds = calculateDocumentMarkerBounds(makeIterator(), markedText);

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

template<typename TextBoxPath>
FloatRect TextBoxPainter<TextBoxPath>::computePaintRect(const LayoutPoint& paintOffset)
{
    FloatPoint localPaintOffset(paintOffset);

    if (m_selectableRange.truncation) {
        if (m_renderer.containingBlock()->style().direction() != textBox().direction()) {
            // Make the visible fragment of text hug the edge closest to the rest of the run by moving the origin
            // at which we start drawing text.
            // e.g. In the case of LTR text truncated in an RTL Context, the correct behavior is:
            // |Hello|CBA| -> |...He|CBA|
            // In order to draw the fragment "He" aligned to the right edge of it's box, we need to start drawing
            // farther to the right.
            // NOTE: WebKit's behavior differs from that of IE which appears to just overlay the ellipsis on top of the
            // truncated string i.e.  |Hello|CBA| -> |...lo|CBA|
            LayoutUnit widthOfVisibleText { m_renderer.width(textBox().start(), *m_selectableRange.truncation, textPosition(), m_isFirstLine) };
            LayoutUnit widthOfHiddenText { m_logicalRect.width() - widthOfVisibleText };
            LayoutSize truncationOffset(textBox().direction() == TextDirection::LTR ? widthOfHiddenText : -widthOfHiddenText, 0_lu);
            localPaintOffset.move(textBox().isHorizontal() ? truncationOffset : truncationOffset.transposedSize());
        }
    }

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
    auto ascent = font.metricsOfPrimaryFont().ascent();
    auto fontSize = std::min(std::max(font.size(), 10.0f), 40.0f);
    auto y = ascent + 0.11035 * fontSize;
    auto height = 0.13247 * fontSize;

    // Avoid measuring the text when the entire line box is selected as an optimization.
    if (markedText.startOffset || markedText.endOffset != textBox->selectableRange().clamp(textBox->end())) {
        auto run = textBox->textRun();
        auto selectionRect = LayoutRect { 0_lu, y, 0_lu, height };
        font.adjustSelectionRectForText(run, selectionRect, markedText.startOffset, markedText.endOffset);
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
    FloatPoint textOrigin { paintRect.x(), paintRect.y() + fontCascade().metricsOfPrimaryFont().ascent() };
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
