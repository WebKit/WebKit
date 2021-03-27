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
#include "MarkedTextStyle.h"

#include "ElementRuleCollector.h"
#include "RenderElement.h"
#include "RenderText.h"
#include "RenderTheme.h"

namespace WebCore {

static StyledMarkedText resolveStyleForMarkedText(const MarkedText& markedText, const MarkedTextStyle& baseStyle, const RenderText& renderer, const RenderStyle& lineStyle, const PaintInfo& paintInfo)
{
    MarkedTextStyle style = baseStyle;
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
            style.backgroundColor = renderStyle->backgroundColor();
            style.textStyles.fillColor = renderStyle->computedStrokeColor();
            style.textStyles.strokeColor = renderStyle->computedStrokeColor();

            auto color = TextDecorationPainter::decorationColor(*renderStyle.get());
            auto decorationStyle = renderStyle->textDecorationStyle();
            auto decorations = renderStyle->textDecorationsInEffect();

            if (decorations.contains(TextDecoration::Underline)) {
                style.textDecorationStyles.underlineColor = color;
                style.textDecorationStyles.underlineStyle = decorationStyle;
            }
            if (decorations.contains(TextDecoration::Overline)) {
                style.textDecorationStyles.overlineColor = color;
                style.textDecorationStyles.overlineStyle = decorationStyle;
            }
            if (decorations.contains(TextDecoration::LineThrough)) {
                style.textDecorationStyles.linethroughColor = color;
                style.textDecorationStyles.linethroughStyle = decorationStyle;
            }
        }
        break;
#if ENABLE(APP_HIGHLIGHTS)
    case MarkedText::AppHighlight: {
        OptionSet<StyleColor::Options> styleColorOptions = { StyleColor::Options::UseSystemAppearance };
        style.backgroundColor = renderer.theme().appHighlightColor(styleColorOptions);
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
        OptionSet<StyleColor::Options> styleColorOptions = { StyleColor::Options::UseSystemAppearance };
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

static MarkedTextStyle computeStyleForUnmarkedMarkedText(const RenderText& renderer, const RenderStyle& lineStyle, bool isFirstLine, const PaintInfo& paintInfo)
{
    MarkedTextStyle style;
    style.textDecorationStyles = TextDecorationPainter::stylesForRenderer(renderer, lineStyle.textDecorationsInEffect(), isFirstLine);
    style.textStyles = computeTextPaintStyle(renderer.frame(), lineStyle, paintInfo);
    style.textShadow = ShadowData::clone(paintInfo.forceTextColor() ? nullptr : lineStyle.textShadow());
    style.alpha = 1;
    return style;
}

Vector<StyledMarkedText> subdivideAndResolveStyle(const Vector<MarkedText>& textsToSubdivide, const RenderText& renderer, bool isFirstLine, const PaintInfo& paintInfo)
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

    auto markedTexts = subdivide(textsToSubdivide);
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

Vector<StyledMarkedText> coalesceAdjacentMarkedTexts(const Vector<StyledMarkedText>& textsToCoalesce, MarkedTextStylesEqualityFunction areMarkedTextStylesEqual)
{
    if (textsToCoalesce.size() <= 1)
        return textsToCoalesce;

    auto areAdjacentMarkedTextsWithSameStyle = [&] (const StyledMarkedText& a, const StyledMarkedText& b) {
        return a.endOffset == b.startOffset && areMarkedTextStylesEqual(a.style, b.style);
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

}
