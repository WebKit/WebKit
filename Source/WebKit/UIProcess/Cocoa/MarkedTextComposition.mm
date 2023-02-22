/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MarkedTextComposition.h"

#import <WebCore/ColorCocoa.h>
#import <pal/spi/cocoa/NSAttributedStringSPI.h>

namespace WebKit {

enum class MarkedTextCompositionMode : uint8_t {
    None, Underline, Highlight
};

template<typename T>
static bool compositionRangeComparator(const T& a, const T& b)
{
    if (a.startOffset < b.startOffset)
        return true;
    if (a.startOffset > b.startOffset)
        return false;
    return a.endOffset < b.endOffset;
}

static MarkedTextCompositionMode markedTextCompositionModeForString(NSAttributedString *string)
{
    __block MarkedTextCompositionMode mode = MarkedTextCompositionMode::None;

    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange, BOOL *stop) {
        BOOL hasUnderlineStyle = !![attributes objectForKey:NSUnderlineStyleAttributeName];
        BOOL hasUnderlineColor = !![attributes objectForKey:NSUnderlineColorAttributeName];

        BOOL hasBackgroundColor = !![attributes objectForKey:NSBackgroundColorAttributeName];
        BOOL hasForegroundColor = !![attributes objectForKey:NSForegroundColorAttributeName];

        // Marked text may be represented either as an underline or a highlight; this mode is dictated
        // by the attributes it has, and therefore having both types of attributes is not allowed.
        ASSERT(!((hasUnderlineStyle || hasUnderlineColor) && (hasBackgroundColor || hasForegroundColor)));

        if (hasUnderlineStyle || hasUnderlineColor) {
            mode = MarkedTextCompositionMode::Underline;
            *stop = YES;
        } else if (hasBackgroundColor || hasForegroundColor) {
            mode = MarkedTextCompositionMode::Highlight;
            *stop = YES;
        }
    }];

    return mode;
}

static Vector<WebCore::CompositionUnderline> extractUnderlines(NSAttributedString *string)
{
    __block Vector<WebCore::CompositionUnderline> underlines;
    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        NSNumber *style = [attributes objectForKey:NSUnderlineStyleAttributeName];
        CocoaColor *cocoaColor = [attributes objectForKey:NSUnderlineColorAttributeName];
        if (!style || !cocoaColor)
            return;

        WebCore::Color underlineColor = WebCore::colorFromCocoaColor(cocoaColor);
        underlines.append({ static_cast<unsigned>(range.location), static_cast<unsigned>(NSMaxRange(range)), WebCore::CompositionUnderlineColor::GivenColor, underlineColor, style.intValue > 1 });
    }];

    if (underlines.isEmpty())
        return { };

    std::sort(underlines.begin(), underlines.end(), &compositionRangeComparator<WebCore::CompositionUnderline>);

    Vector<WebCore::CompositionUnderline> mergedUnderlines;

    auto firstInactiveUnderlineIndex = underlines.findIf([](auto& underline) {
        return !underline.thick;
    });

    if (firstInactiveUnderlineIndex != notFound) {
        auto& firstInactiveUnderline = underlines[firstInactiveUnderlineIndex];
        firstInactiveUnderline.startOffset = underlines.first().startOffset;
        firstInactiveUnderline.endOffset = underlines.last().endOffset;
        mergedUnderlines.append(firstInactiveUnderline);
    }

    for (auto& underline : underlines) {
        if (underline.thick)
            mergedUnderlines.append(underline);
    }

    return mergedUnderlines;
}

static Vector<WebCore::CompositionHighlight> compositionHighlights(NSAttributedString *string)
{
    if (!string.length)
        return { };

    __block Vector<WebCore::CompositionHighlight> highlights;
    [string enumerateAttributesInRange:NSMakeRange(0, string.length) options:0 usingBlock:^(NSDictionary<NSAttributedStringKey, id> *attributes, NSRange range, BOOL *) {
        std::optional<WebCore::Color> backgroundHighlightColor;
        std::optional<WebCore::Color> foregroundHighlightColor;

        if (CocoaColor *backgroundColor = attributes[NSBackgroundColorAttributeName])
            backgroundHighlightColor = WebCore::colorFromCocoaColor(backgroundColor);

        if (CocoaColor *foregroundColor = attributes[NSForegroundColorAttributeName])
            foregroundHighlightColor = WebCore::colorFromCocoaColor(foregroundColor);

        highlights.append({ static_cast<unsigned>(range.location), static_cast<unsigned>(NSMaxRange(range)), backgroundHighlightColor, foregroundHighlightColor });
    }];

    std::sort(highlights.begin(), highlights.end(), &compositionRangeComparator<WebCore::CompositionHighlight>);

    Vector<WebCore::CompositionHighlight> mergedHighlights;
    mergedHighlights.reserveInitialCapacity(highlights.size());
    for (auto& highlight : highlights) {
        if (mergedHighlights.isEmpty() || mergedHighlights.last().backgroundColor != highlight.backgroundColor || mergedHighlights.last().foregroundColor != highlight.foregroundColor)
            mergedHighlights.append(highlight);
        else
            mergedHighlights.last().endOffset = highlight.endOffset;
    }

    return mergedHighlights;
}

MarkedTextComposition extractMarkedTextComposition(NSAttributedString *string)
{
    if (!string.length)
        return { };

    auto mode = markedTextCompositionModeForString(string);

    switch (mode) {
    case MarkedTextCompositionMode::None:
        return { };

    case MarkedTextCompositionMode::Underline:
        return extractUnderlines(string);

    case MarkedTextCompositionMode::Highlight:
        return compositionHighlights(string);
    }
}

} // namespace WebKit
