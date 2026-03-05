/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyledMarkedText.h"

#include "ColorBlending.h"
#include "ElementRuleCollector.h"
#include "RenderElement.h"
#include "RenderStyle+GettersInlines.h"
#include "RenderText.h"
#include "RenderTheme.h"

namespace WebCore {

static void computeStyleForPseudoElementStyle(StyledMarkedText::Style& style, const RenderStyle* pseudoElementStyle, const PaintInfo& paintInfo)
{
    if (!pseudoElementStyle)
        return;

    style.backgroundColor = pseudoElementStyle->visitedDependentBackgroundColorApplyingColorFilter(paintInfo.paintBehavior);
    style.textStyles.fillColor = pseudoElementStyle->usedStrokeColor();
    style.textStyles.strokeColor = pseudoElementStyle->usedStrokeColor();
    style.textStyles.hasExplicitlySetFillColor = pseudoElementStyle->hasExplicitlySetColor();

    auto color = TextDecorationPainter::decorationColor(*pseudoElementStyle, paintInfo.paintBehavior);
    auto decorationStyle = pseudoElementStyle->textDecorationStyle();
    auto decorations = pseudoElementStyle->textDecorationLineInEffect();

    if (decorations.hasUnderline()) {
        style.textDecorationStyles.underline.color = color;
        style.textDecorationStyles.underline.decorationStyle = decorationStyle;
    }
    if (decorations.hasOverline()) {
        style.textDecorationStyles.overline.color = color;
        style.textDecorationStyles.overline.decorationStyle = decorationStyle;
    }
    if (decorations.hasLineThrough()) {
        style.textDecorationStyles.linethrough.color = color;
        style.textDecorationStyles.linethrough.decorationStyle = decorationStyle;
    }
}

static StyledMarkedText resolveStyleForMarkedText(const MarkedText& markedText, const StyledMarkedText::Style& baseStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    static constexpr OptionSet systemAppearanceOptions { StyleColorOptions::UseSystemAppearance };

    auto style = baseStyle;
    switch (markedText.type) {
    case MarkedText::Type::Correction:
    case MarkedText::Type::DictationAlternatives:
#if PLATFORM(IOS_FAMILY)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
    case MarkedText::Type::DictationPhraseWithAlternatives:
#endif
#if ENABLE(WRITING_TOOLS)
    case MarkedText::Type::WritingToolsTextSuggestion:
#endif
    case MarkedText::Type::Unmarked:
        break;
    case MarkedText::Type::GrammarError: {
        auto* renderStyle = renderer.grammarErrorPseudoStyle();
        computeStyleForPseudoElementStyle(style, renderStyle, paintInfo);
        break;
    }
    case MarkedText::Type::Highlight: {
        auto renderStyle = renderer.parent()->getUncachedPseudoStyle({ PseudoElementType::Highlight, markedText.highlightName }, &renderer.style());
        computeStyleForPseudoElementStyle(style, renderStyle.get(), paintInfo);
        break;
    }
    case MarkedText::Type::SpellingError: {
        auto* renderStyle = renderer.spellingErrorPseudoStyle();
        computeStyleForPseudoElementStyle(style, renderStyle, paintInfo);
        break;
    }
    case MarkedText::Type::FragmentHighlight: {
        if (CheckedPtr renderStyle = renderer.targetTextPseudoStyle()) {
            computeStyleForPseudoElementStyle(style, renderStyle.get(), paintInfo);
            break;
        }

        style.backgroundColor = renderer.theme().annotationHighlightBackgroundColor(systemAppearanceOptions);
        style.textStyles.fillColor = renderer.theme().annotationHighlightForegroundColor(systemAppearanceOptions);
        break;
    }
    case MarkedText::Type::TextExtractionHighlight: {
        // FIXME: This just uses the same color as annotation highlights for now, but we may eventually require a different appearance.
        style.backgroundColor = renderer.theme().annotationHighlightBackgroundColor(systemAppearanceOptions);
        style.textStyles.fillColor = renderer.theme().annotationHighlightForegroundColor(systemAppearanceOptions);
        break;
    }
#if ENABLE(APP_HIGHLIGHTS)
    case MarkedText::Type::AppHighlight: {
        style.backgroundColor = renderer.theme().annotationHighlightBackgroundColor(systemAppearanceOptions);
        style.textStyles.fillColor = renderer.theme().annotationHighlightForegroundColor(systemAppearanceOptions);
        break;
    }
#endif
    case MarkedText::Type::DraggedContent:
        style.alpha = 0.25;
        break;
    case MarkedText::Type::TransparentContent:
        style.alpha = 0.0;
        break;
    case MarkedText::Type::Selection: {
        style.textStyles = computeTextSelectionPaintStyle(style.textStyles, renderer, lineStyle, paintInfo, style.textShadow);

        Color selectionBackgroundColor = renderer.selectionBackgroundColor();
        style.backgroundColor = selectionBackgroundColor;
        if (selectionBackgroundColor.isValid() && selectionBackgroundColor.isVisible() && style.textStyles.fillColor == selectionBackgroundColor)
            style.backgroundColor = selectionBackgroundColor.invertedColorWithAlpha(1.0);
        break;
    }
    case MarkedText::Type::TextMatch: {
        // Text matches always use the light system appearance.
#if PLATFORM(MAC)
        style.textStyles.fillColor = renderer.theme().systemColor(CSSValueAppleSystemLabel, systemAppearanceOptions);
#endif
        style.backgroundColor = renderer.theme().textSearchHighlightColor(systemAppearanceOptions);
        break;
    }
    }
    StyledMarkedText styledMarkedText = markedText;
    styledMarkedText.style = WTF::move(style);
    return styledMarkedText;
}

StyledMarkedText::Style StyledMarkedText::computeStyleForUnmarkedMarkedText(const RenderText& renderer, const RenderStyle& lineStyle, bool isFirstLine, const PaintInfo& paintInfo)
{
    StyledMarkedText::Style style;
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer, lineStyle.textDecorationLineInEffect(), isFirstLine, paintInfo.paintBehavior);
    style.textStyles = computeTextPaintStyle(renderer, lineStyle, paintInfo);
    style.textShadow = paintInfo.forceTextColor() ? WebCore::Style::TextShadows { CSS::Keyword::None { } } : lineStyle.textShadow();
    return style;
}

static TextDecorationPainter::Styles computeStylesForTextDecorations(const TextDecorationPainter::Styles& previousTextDecorationStyles, const TextDecorationPainter::Styles& currentTextDecorationStyles)
{
    auto textDecorations = TextDecorationPainter::textDecorationsInEffectForStyle(currentTextDecorationStyles);

    if (textDecorations.isNone())
        return previousTextDecorationStyles;

    auto textDecorationStyles = previousTextDecorationStyles;

    if (textDecorations.hasUnderline()) {
        textDecorationStyles.underline.color = currentTextDecorationStyles.underline.color;
        textDecorationStyles.underline.decorationStyle = currentTextDecorationStyles.underline.decorationStyle;
    }
    if (textDecorations.hasOverline()) {
        textDecorationStyles.overline.color = currentTextDecorationStyles.overline.color;
        textDecorationStyles.overline.decorationStyle = currentTextDecorationStyles.overline.decorationStyle;
    }
    if (textDecorations.hasLineThrough()) {
        textDecorationStyles.linethrough.color = currentTextDecorationStyles.linethrough.color;
        textDecorationStyles.linethrough.decorationStyle = currentTextDecorationStyles.linethrough.decorationStyle;
    }
    return textDecorationStyles;
}

static Vector<StyledMarkedText> coalesceAdjacentWithSameRanges(Vector<StyledMarkedText>&& styledTexts)
{
    ASSERT(!styledTexts.isEmpty());
    Vector<StyledMarkedText> frontmostMarkedTexts;
    frontmostMarkedTexts.append(styledTexts[0]);
    for (size_t i = 1; i < styledTexts.size(); ++i) {
        auto& text = styledTexts[i];
        auto& previousStyledMarkedText = frontmostMarkedTexts.last();
        // StyledMarkedTexts completely cover each other.
        if (previousStyledMarkedText.startOffset == text.startOffset && previousStyledMarkedText.endOffset == text.endOffset) {
            // If either background for two different custom highlight StyledMarkedTexts are not opaque, blend colors together.
            if (previousStyledMarkedText.highlightName != text.highlightName
                && (!previousStyledMarkedText.style.backgroundColor.isOpaque()
                    || !text.style.backgroundColor.isOpaque()
                    || (text.highlightName.isNull() && text.style.backgroundColor.isVisible())))
                        previousStyledMarkedText.style.backgroundColor = blendSourceOver(previousStyledMarkedText.style.backgroundColor, text.style.backgroundColor);
            // Take text color of StyledMarkedText, maintaining insertion and priority order.
            if (text.type != MarkedText::Type::Unmarked && text.style.textStyles.hasExplicitlySetFillColor)
                previousStyledMarkedText.style.textStyles.fillColor = text.style.textStyles.fillColor;
            // Take the highlightName of the latest StyledMarkedText, regardless of priority.
            if (!text.highlightName.isNull())
                previousStyledMarkedText.highlightName = text.highlightName;

            if (previousStyledMarkedText.priority <= text.priority) {
                previousStyledMarkedText.priority = text.priority;
                // If highlight, combine textDecorationStyles accordingly.
                // FIXME: Check for taking textDecorationStyles needs to accommodate other MarkedText type.
                if (!text.highlightName.isNull())
                    previousStyledMarkedText.style.textDecorationStyles = computeStylesForTextDecorations(previousStyledMarkedText.style.textDecorationStyles, text.style.textDecorationStyles);
                // If higher or same priority and opaque, override background color.
                if (text.style.backgroundColor.isOpaque())
                    previousStyledMarkedText.style.backgroundColor = text.style.backgroundColor;
            }
            continue;
        }
        frontmostMarkedTexts.append(WTF::move(text));
    }
    return frontmostMarkedTexts;
}

static void orderHighlights(const ListHashSet<AtomString>& markedTextsNames, Vector<MarkedText>& markedTexts)
{
    if (markedTexts.isEmpty())
        return;

    HashMap<AtomString, int> markedTextsNamesPriority;
    int index = 0;
    for (auto& highlightName : markedTextsNames) {
        markedTextsNamesPriority.add(highlightName, index);
        index++;
    }

    index = 0;
    while (index < static_cast<int>(markedTexts.size() - 1)) {
        // If two adjacent highlights with same ranges are not in correct priority order, swap them and move on.
        if (!markedTexts[index].highlightName.isNull()
            && !markedTexts[index + 1].highlightName.isNull()
            && markedTextsNamesPriority.get(markedTexts[index].highlightName) > markedTextsNamesPriority.get(markedTexts[index + 1].highlightName)
            && markedTexts[index].startOffset == markedTexts[index + 1].startOffset
            && markedTexts[index].endOffset == markedTexts[index + 1].endOffset) {
            std::swap(markedTexts[index], markedTexts[index + 1]);
        }
        ++index;
    }
}

Vector<StyledMarkedText> StyledMarkedText::subdivideAndResolve(const Vector<MarkedText>& textsToSubdivide, const RenderText& renderer, bool isFirstLine, const PaintInfo& paintInfo)
{
    if (textsToSubdivide.isEmpty())
        return { };

    // Keep track of original order of highlights.
    ListHashSet<AtomString> markedTextsNames;
    for (auto& markedText : textsToSubdivide) {
        if (!markedText.highlightName.isNull())
            markedTextsNames.add(markedText.highlightName);
    }

    auto& lineStyle = isFirstLine ? renderer.firstLineStyle() : renderer.style();
    auto baseStyle = computeStyleForUnmarkedMarkedText(renderer, lineStyle, isFirstLine, paintInfo);

    if (textsToSubdivide.size() == 1 && textsToSubdivide[0].type == MarkedText::Type::Unmarked) {
        StyledMarkedText styledMarkedText = textsToSubdivide[0];
        styledMarkedText.style = WTF::move(baseStyle);
        return { styledMarkedText };
    }

    auto markedTexts = MarkedText::subdivide(textsToSubdivide, OverlapStrategy::None);
    ASSERT(!markedTexts.isEmpty());
    if (markedTexts.isEmpty()) [[unlikely]]
        return { };

    if (!markedTexts.isEmpty()) {
        // Check if vector contains custom highlights.
        bool containsHighlights = markedTexts.containsIf([](const auto& item) {
            return item.type == MarkedText::Type::Highlight;
        });

        // Sort custom highlights to follow correct priority/insertion order.
        if (containsHighlights) {
            orderHighlights(markedTextsNames, markedTexts);

            auto frontmostMarkedTexts = WTF::map(markedTexts, [&](auto& markedText) {
                return resolveStyleForMarkedText(markedText, baseStyle, renderer, lineStyle, paintInfo);
            });

            return coalesceAdjacentWithSameRanges(WTF::move(frontmostMarkedTexts));
        }
    }

    // Compute frontmost overlapping styled marked texts.
    Vector<StyledMarkedText> frontmostMarkedTexts;
    frontmostMarkedTexts.reserveInitialCapacity(markedTexts.size());
    frontmostMarkedTexts.append(resolveStyleForMarkedText(markedTexts[0], baseStyle, renderer, lineStyle, paintInfo));
    for (size_t i = 1; i < markedTexts.size(); ++i) {
        auto& text = markedTexts[i];
        auto& previousStyledMarkedText = frontmostMarkedTexts.last();
        // Marked texts completely cover each other.
        if (previousStyledMarkedText.startOffset == text.startOffset && previousStyledMarkedText.endOffset == text.endOffset) {
            previousStyledMarkedText = resolveStyleForMarkedText(text, previousStyledMarkedText.style, renderer, lineStyle, paintInfo);
            continue;
        }
        frontmostMarkedTexts.append(resolveStyleForMarkedText(text, baseStyle, renderer, lineStyle, paintInfo));
    }

    return frontmostMarkedTexts;
}

template<typename EqualityFunction>
static Vector<StyledMarkedText> coalesceAdjacent(const Vector<StyledMarkedText>& textsToCoalesce, EqualityFunction&& equalityFunction)
{
    if (textsToCoalesce.size() <= 1)
        return textsToCoalesce;

    auto areAdjacentMarkedTextsWithSameStyle = [&] (const StyledMarkedText& a, const StyledMarkedText& b) {
        return a.endOffset == b.startOffset && equalityFunction(a.style, b.style);
    };

    Vector<StyledMarkedText> styledMarkedTexts;
    styledMarkedTexts.reserveInitialCapacity(textsToCoalesce.size());
    styledMarkedTexts.append(textsToCoalesce[0]);
    for (size_t i = 1; i < textsToCoalesce.size(); ++i) {
        auto& text = textsToCoalesce[i];
        auto& previousStyledMarkedText = styledMarkedTexts.last();
        if (areAdjacentMarkedTextsWithSameStyle(previousStyledMarkedText, text)) {
            previousStyledMarkedText.endOffset = text.endOffset;
            continue;
        }
        styledMarkedTexts.append(text);
    }

    return styledMarkedTexts;
}

Vector<StyledMarkedText> StyledMarkedText::coalesceAdjacentWithEqualBackground(const Vector<StyledMarkedText>& markedTexts)
{
    return coalesceAdjacent(markedTexts, [&](auto& a, auto& b) {
        return a.backgroundColor == b.backgroundColor;
    });
}

Vector<StyledMarkedText> StyledMarkedText::coalesceAdjacentWithEqualForeground(const Vector<StyledMarkedText>& markedTexts)
{
    return coalesceAdjacent(markedTexts, [&](auto& a, auto& b) {
        return a.textStyles == b.textStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    });
}

Vector<StyledMarkedText> StyledMarkedText::coalesceAdjacentWithEqualDecorations(const Vector<StyledMarkedText>& markedTexts)
{
    return coalesceAdjacent(markedTexts, [&](auto& a, auto& b) {
        return a.textDecorationStyles == b.textDecorationStyles && a.textStyles == b.textStyles && a.textShadow == b.textShadow && a.alpha == b.alpha;
    });
}
}
