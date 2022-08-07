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

#include "ElementRuleCollector.h"
#include "RenderElement.h"
#include "RenderText.h"
#include "RenderTheme.h"

namespace WebCore {

static StyledMarkedText resolveStyleForMarkedText(const MarkedText& markedText, const StyledMarkedText::Style& baseStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    auto style = baseStyle;
    switch (markedText.type) {
    case MarkedText::Correction:
    case MarkedText::DictationAlternatives:
#if PLATFORM(IOS_FAMILY)
    // FIXME: See <rdar://problem/8933352>. Also, remove the PLATFORM(IOS_FAMILY)-guard.
    case MarkedText::DictationPhraseWithAlternatives:
#endif
    case MarkedText::GrammarError:
    case MarkedText::SpellingError:
    case MarkedText::Unmarked:
        break;
    case MarkedText::Highlight:
        if (auto renderStyle = renderer.parent()->getUncachedPseudoStyle({ PseudoId::Highlight, markedText.highlightName }, &renderer.style())) {
            style.backgroundColor = renderStyle->colorResolvingCurrentColor(renderStyle->backgroundColor());
            style.textStyles.fillColor = renderStyle->computedStrokeColor();
            style.textStyles.strokeColor = renderStyle->computedStrokeColor();

            auto color = TextDecorationPainter::decorationColor(*renderStyle.get());
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
    case MarkedText::FragmentHighlight: {
        OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
        style.backgroundColor = renderer.theme().annotationHighlightColor(styleColorOptions);
        break;
    }
#if ENABLE(APP_HIGHLIGHTS)
    case MarkedText::AppHighlight: {
        OptionSet<StyleColorOptions> styleColorOptions = { StyleColorOptions::UseSystemAppearance };
        style.backgroundColor = renderer.theme().annotationHighlightColor(styleColorOptions);
        break;
    }
#endif
    case MarkedText::DraggedContent:
        style.alpha = 0.25;
        break;
    case MarkedText::Selection: {
        style.textStyles = computeTextSelectionPaintStyle(style.textStyles, renderer, lineStyle, paintInfo, style.textShadow);

        Color selectionBackgroundColor = renderer.selectionBackgroundColor();
        style.backgroundColor = selectionBackgroundColor;
        if (selectionBackgroundColor.isValid() && selectionBackgroundColor.isVisible() && style.textStyles.fillColor == selectionBackgroundColor)
            style.backgroundColor = selectionBackgroundColor.invertedColorWithAlpha(1.0);
        break;
    }
    case MarkedText::TextMatch: {
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
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer, lineStyle.textDecorationsInEffect(), isFirstLine);
    style.textStyles = computeTextPaintStyle(renderer.frame(), lineStyle, paintInfo);
    style.textShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
    return style;
}

Vector<StyledMarkedText> StyledMarkedText::subdivideAndResolve(const Vector<MarkedText>& textsToSubdivide, const RenderText& renderer, bool isFirstLine, const PaintInfo& paintInfo)
{
    if (textsToSubdivide.isEmpty())
        return { };

    Vector<StyledMarkedText> frontmostMarkedTexts;

    auto& lineStyle = isFirstLine ? renderer.firstLineStyle() : renderer.style();
    auto baseStyle = computeStyleForUnmarkedMarkedText(renderer, lineStyle, isFirstLine, paintInfo);

    if (textsToSubdivide.size() == 1 && textsToSubdivide[0].type == MarkedText::Unmarked) {
        StyledMarkedText styledMarkedText = textsToSubdivide[0];
        styledMarkedText.style = WTFMove(baseStyle);
        return { styledMarkedText };
    }

    auto markedTexts = MarkedText::subdivide(textsToSubdivide);
    ASSERT(!markedTexts.isEmpty());
    if (UNLIKELY(markedTexts.isEmpty()))
        return { };

    // Compute frontmost overlapping styled marked texts.
    frontmostMarkedTexts.reserveInitialCapacity(markedTexts.size());
    frontmostMarkedTexts.uncheckedAppend(resolveStyleForMarkedText(markedTexts[0], baseStyle, renderer, lineStyle, paintInfo));
    for (auto it = markedTexts.begin() + 1, end = markedTexts.end(); it != end; ++it) {
        StyledMarkedText& previousStyledMarkedText = frontmostMarkedTexts.last();
        if (previousStyledMarkedText.startOffset == it->startOffset && previousStyledMarkedText.endOffset == it->endOffset) {
            // Marked texts completely cover each other.
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
