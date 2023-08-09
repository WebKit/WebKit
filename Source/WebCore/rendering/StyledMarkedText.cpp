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
#include "RenderStyleInlines.h"
#include "RenderText.h"
#include "RenderTheme.h"

namespace WebCore {

static StyledMarkedText resolveStyleForMarkedText(const MarkedText& markedText, const StyledMarkedText::Style& baseStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    auto style = baseStyle;
    switch (markedText.type) {
    case MarkedText::Type::Correction:
    case MarkedText::Type::DictationAlternatives:
#if PLATFORM(IOS_FAMILY)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
    case MarkedText::Type::DictationPhraseWithAlternatives:
#endif
    case MarkedText::Type::GrammarError:
    case MarkedText::Type::SpellingError:
    case MarkedText::Type::Unmarked:
        break;
    case MarkedText::Type::Highlight:
        if (auto renderStyle = renderer.parent()->getUncachedPseudoStyle({ PseudoId::Highlight, markedText.highlightName }, &renderer.style())) {
            style.backgroundColor = renderStyle->colorResolvingCurrentColor(renderStyle->backgroundColor());
            style.textStyles.fillColor = renderStyle->computedStrokeColor();
            style.textStyles.strokeColor = renderStyle->computedStrokeColor();
            style.textStyles.hasExplicitlySetFillColor = renderStyle->hasExplicitlySetColor();

            auto color = TextDecorationPainter::decorationColor(*renderStyle.get(), paintInfo.paintBehavior);
            auto decorationStyle = renderStyle->textDecorationStyle();
            auto decorations = renderStyle->textDecorationsInEffect();

            if (decorations.contains(TextDecorationLine::Underline)) {
                style.textDecorationStyles.underline.color = color;
                style.textDecorationStyles.underline.decorationStyle = decorationStyle;
            }
            if (decorations.contains(TextDecorationLine::Overline)) {
                style.textDecorationStyles.overline.color = color;
                style.textDecorationStyles.overline.decorationStyle = decorationStyle;
            }
            if (decorations.contains(TextDecorationLine::LineThrough)) {
                style.textDecorationStyles.linethrough.color = color;
                style.textDecorationStyles.linethrough.decorationStyle = decorationStyle;
            }
        }
        break;
    case MarkedText::Type::FragmentHighlight: {
        OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
        style.backgroundColor = renderer.theme().annotationHighlightColor(styleColorOptions);
        break;
    }
#if ENABLE(APP_HIGHLIGHTS)
    case MarkedText::Type::AppHighlight: {
        OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
        style.backgroundColor = renderer.theme().annotationHighlightColor(styleColorOptions);
        break;
    }
#endif
    case MarkedText::Type::DraggedContent:
        style.alpha = 0.25;
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
        OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
#if PLATFORM(MAC)
        style.textStyles.fillColor = renderer.theme().systemColor(CSSValueAppleSystemLabel, styleColorOptions);
#endif
        style.backgroundColor = renderer.theme().textSearchHighlightColor(styleColorOptions);
        break;
    }
    }
    StyledMarkedText styledMarkedText = markedText;
    styledMarkedText.style = WTFMove(style);
    return styledMarkedText;
}

StyledMarkedText::Style StyledMarkedText::computeStyleForUnmarkedMarkedText(const RenderText& renderer, const RenderStyle& lineStyle, bool isFirstLine, const PaintInfo& paintInfo)
{
    StyledMarkedText::Style style;
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer, lineStyle.textDecorationsInEffect(), isFirstLine, paintInfo.paintBehavior);
    style.textStyles = computeTextPaintStyle(renderer.frame(), lineStyle, paintInfo);
    style.textShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
    return style;
}

static TextDecorationPainter::Styles computeStylesForTextDecorations(const TextDecorationPainter::Styles& previousTextDecorationStyles, const TextDecorationPainter::Styles& currentTextDecorationStyles)
{
    auto textDecorations = TextDecorationPainter::textDecorationsInEffectForStyle(currentTextDecorationStyles);

    if (textDecorations.isEmpty())
        return previousTextDecorationStyles;

    auto textDecorationStyles = previousTextDecorationStyles;

    if (textDecorations.contains(TextDecorationLine::Underline)) {
        textDecorationStyles.underline.color = currentTextDecorationStyles.underline.color;
        textDecorationStyles.underline.decorationStyle = currentTextDecorationStyles.underline.decorationStyle;
    }
    if (textDecorations.contains(TextDecorationLine::Overline)) {
        textDecorationStyles.overline.color = currentTextDecorationStyles.overline.color;
        textDecorationStyles.overline.decorationStyle = currentTextDecorationStyles.overline.decorationStyle;
    }
    if (textDecorations.contains(TextDecorationLine::LineThrough)) {
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
    for (auto it = styledTexts.begin() + 1, end = styledTexts.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = frontmostMarkedTexts.last();
        // StyledMarkedTexts completely cover each other.
        if (previousStyledMarkedText.startOffset == it->startOffset && previousStyledMarkedText.endOffset == it->endOffset) {
            // If either background for two different custom highlight StyledMarkedTexts are not opaque, blend colors together.
            if (previousStyledMarkedText.highlightName != it->highlightName
                && (!previousStyledMarkedText.style.backgroundColor.isOpaque()
                    || !it->style.backgroundColor.isOpaque()
                    || (it->highlightName.isNull() && it->style.backgroundColor.isVisible())))
                        previousStyledMarkedText.style.backgroundColor = blendSourceOver(previousStyledMarkedText.style.backgroundColor, it->style.backgroundColor);
            // Take text color of StyledMarkedText, maintaining insertion and priority order.
            if (it->type != MarkedText::Type::Unmarked && it->style.textStyles.hasExplicitlySetFillColor)
                previousStyledMarkedText.style.textStyles.fillColor = it->style.textStyles.fillColor;
            // Take the highlightName of the latest StyledMarkedText, regardless of priority.
            if (!it->highlightName.isNull())
                previousStyledMarkedText.highlightName = it->highlightName;

            if (previousStyledMarkedText.priority <= it->priority) {
                previousStyledMarkedText.priority = it->priority;
                // If highlight, combine textDecorationStyles accordingly.
                // FIXME: Check for taking textDecorationStyles needs to accommodate other MarkedText type.
                if (!it->highlightName.isNull())
                    previousStyledMarkedText.style.textDecorationStyles = computeStylesForTextDecorations(previousStyledMarkedText.style.textDecorationStyles, it->style.textDecorationStyles);
                // If higher or same priority and opaque, override background color.
                if (it->style.backgroundColor.isOpaque())
                    previousStyledMarkedText.style.backgroundColor = it->style.backgroundColor;
            }
            continue;
        }
        frontmostMarkedTexts.append(WTFMove(*it));
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
        styledMarkedText.style = WTFMove(baseStyle);
        return { styledMarkedText };
    }

    auto markedTexts = MarkedText::subdivide(textsToSubdivide, OverlapStrategy::None);
    ASSERT(!markedTexts.isEmpty());
    if (UNLIKELY(markedTexts.isEmpty()))
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

            return coalesceAdjacentWithSameRanges(WTFMove(frontmostMarkedTexts));
        }
    }

    // Compute frontmost overlapping styled marked texts.
    Vector<StyledMarkedText> frontmostMarkedTexts;
    frontmostMarkedTexts.reserveInitialCapacity(markedTexts.size());
    frontmostMarkedTexts.uncheckedAppend(resolveStyleForMarkedText(markedTexts[0], baseStyle, renderer, lineStyle, paintInfo));
    for (auto it = markedTexts.begin() + 1, end = markedTexts.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = frontmostMarkedTexts.last();
        // Marked texts completely cover each other.
        if (previousStyledMarkedText.startOffset == it->startOffset && previousStyledMarkedText.endOffset == it->endOffset) {
            previousStyledMarkedText = resolveStyleForMarkedText(*it, previousStyledMarkedText.style, renderer, lineStyle, paintInfo);
            continue;
        }
        frontmostMarkedTexts.uncheckedAppend(resolveStyleForMarkedText(*it, baseStyle, renderer, lineStyle, paintInfo));
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
    styledMarkedTexts.uncheckedAppend(textsToCoalesce[0]);
    for (auto it = textsToCoalesce.begin() + 1, end = textsToCoalesce.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = styledMarkedTexts.last();
        if (areAdjacentMarkedTextsWithSameStyle(previousStyledMarkedText, *it)) {
            previousStyledMarkedText.endOffset = it->endOffset;
            continue;
        }
        styledMarkedTexts.uncheckedAppend(*it);
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
