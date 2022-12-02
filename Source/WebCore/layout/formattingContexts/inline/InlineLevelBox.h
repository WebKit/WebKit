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

#pragma once

#include "FontCascade.h"
#include "InlineRect.h"
#include "LayoutBox.h"
#include "LayoutUnits.h"
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

class LineBox;
class LineBoxBuilder;
class LineBoxVerticalAligner;

class InlineLevelBox {
public:
    enum class LineSpanningInlineBox { Yes, No };
    static InlineLevelBox createInlineBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox = LineSpanningInlineBox::No);
    static InlineLevelBox createAtomicInlineLevelBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
    static InlineLevelBox createLineBreakBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft);
    static InlineLevelBox createGenericInlineLevelBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft);

    InlineLayoutUnit ascent() const { return m_ascent; }
    std::optional<InlineLayoutUnit> descent() const { return m_descent; }
    // See https://www.w3.org/TR/css-inline-3/#layout-bounds
    struct LayoutBounds {
        InlineLayoutUnit height() const { return ascent + descent; }
        bool operator==(const LayoutBounds& other) const { return ascent == other.ascent && descent == other.descent; }

        InlineLayoutUnit ascent { 0 };
        InlineLayoutUnit descent { 0 };
    };
    std::optional<LayoutBounds> layoutBounds() const { return m_layoutBounds; }

    bool hasContent() const { return m_hasContent; }
    void setHasContent();

    struct VerticalAlignment {
        VerticalAlign type { VerticalAlign::Baseline };
        std::optional<InlineLayoutUnit> baselineOffset;
    };
    VerticalAlignment verticalAlign() const { return m_style.verticalAlignment; }
    bool hasLineBoxRelativeAlignment() const;

    InlineLayoutUnit preferredLineHeight() const;
    bool isPreferredLineHeightFontMetricsBased() const { return m_style.lineHeight.isNegative(); }

    bool lineBoxContain() const;
    bool hasLineBoxContain() const { return m_style.lineBoxContain != RenderStyle::initialLineBoxContain(); }

    const FontMetrics& primarymetricsOfPrimaryFont() const { return m_style.primaryFontMetrics; }
    InlineLayoutUnit fontSize() const { return m_style.primaryFontSize; }

    bool hasAnnotation() const { return hasContent() && m_annotation.has_value(); };
    std::optional<InlineLayoutUnit> annotationAbove() const { return hasAnnotation() && m_annotation->type == Annotation::Type::Above ? std::make_optional(m_annotation->size) : std::nullopt; }
    std::optional<InlineLayoutUnit> annotationUnder() const { return hasAnnotation() && m_annotation->type == Annotation::Type::Under ? std::make_optional(m_annotation->size) : std::nullopt; }

    bool isInlineBox() const { return m_type == Type::InlineBox || isRootInlineBox() || isLineSpanningInlineBox(); }
    bool isRootInlineBox() const { return m_type == Type::RootInlineBox; }
    bool isLineSpanningInlineBox() const { return m_type == Type::LineSpanningInlineBox; }
    bool isAtomicInlineLevelBox() const { return m_type == Type::AtomicInlineLevelBox; }
    bool isListMarker() const { return isAtomicInlineLevelBox() && layoutBox().isListMarkerBox(); }
    bool isLineBreakBox() const { return m_type == Type::LineBreakBox; }

    enum class Type : uint8_t {
        InlineBox             = 1 << 0,
        LineSpanningInlineBox = 1 << 1,
        RootInlineBox         = 1 << 2,
        AtomicInlineLevelBox  = 1 << 3,
        LineBreakBox          = 1 << 4,
        GenericInlineLevelBox = 1 << 5
    };
    Type type() const { return m_type; }

    const Box& layoutBox() const { return m_layoutBox; }

    bool isFirstBox() const { return m_isFirstWithinLayoutBox; }
    bool isLastBox() const { return m_isLastWithinLayoutBox; }

private:
    enum class PositionWithinLayoutBox {
        First = 1 << 0,
        Last  = 1 << 1
    };
    InlineLevelBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutSize, Type, OptionSet<PositionWithinLayoutBox> = { PositionWithinLayoutBox::First, PositionWithinLayoutBox::Last });

    friend class InlineDisplayLineBuilder;
    friend class LineBox;
    friend class LineBoxBuilder;
    friend class LineBoxVerticalAligner;
    friend class InlineFormattingGeometry;

    const InlineRect& logicalRect() const { return m_logicalRect; }
    InlineLayoutUnit logicalTop() const { return m_logicalRect.top(); }
    InlineLayoutUnit logicalBottom() const { return m_logicalRect.bottom(); }
    InlineLayoutUnit logicalLeft() const { return m_logicalRect.left(); }
    InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

    // FIXME: Remove legacy rounding.
    void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(roundToInt(logicalHeight)); }
    void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop >= 0 ? roundToInt(logicalTop) : -roundToInt(-logicalTop)); }
    void setAscent(InlineLayoutUnit ascent) { m_ascent = roundToInt(ascent); }
    void setDescent(InlineLayoutUnit descent) { m_descent = roundToInt(descent); }
    void setLayoutBounds(const LayoutBounds& layoutBounds) { m_layoutBounds = { InlineLayoutUnit(roundToInt(layoutBounds.ascent)), InlineLayoutUnit(roundToInt(layoutBounds.descent)) }; }

    void setIsFirstBox() { m_isFirstWithinLayoutBox = true; }
    void setIsLastBox() { m_isLastWithinLayoutBox = true; }

private:
    CheckedRef<const Box> m_layoutBox;
    // This is the combination of margin and border boxes. Inline level boxes are vertically aligned using their margin boxes.
    InlineRect m_logicalRect;
    std::optional<LayoutBounds> m_layoutBounds { };
    InlineLayoutUnit m_ascent { 0 };
    std::optional<InlineLayoutUnit> m_descent;
    bool m_hasContent { false };
    // These bits are about whether this inline level box is the first/last generated box of the associated Layout::Box
    // (e.g. always true for atomic inline level boxes, but inline boxes spanning over multiple lines can produce separate first/last boxes).
    bool m_isFirstWithinLayoutBox { false };
    bool m_isLastWithinLayoutBox { false };
    Type m_type { Type::InlineBox };

    struct Style {
        const FontMetrics& primaryFontMetrics;
        const Length& lineHeight;
        WTF::OptionSet<LineBoxContain> lineBoxContain;
        InlineLayoutUnit primaryFontSize { 0 };
        VerticalAlignment verticalAlignment { };
    };
    Style m_style;

    struct Annotation {
        enum class Type : uint8_t { Above, Under };
        Type type { Type::Above };
        InlineLayoutUnit size { };
    };
    std::optional<Annotation> m_annotation;
};

inline InlineLevelBox::InlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize, Type type, OptionSet<PositionWithinLayoutBox> positionWithinLayoutBox)
    : m_layoutBox(layoutBox)
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_isFirstWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::First))
    , m_isLastWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::Last))
    , m_type(type)
    , m_style({ style.fontCascade().metricsOfPrimaryFont(), style.lineHeight(), style.lineBoxContain(), InlineLayoutUnit(style.fontCascade().fontDescription().computedPixelSize()), { } })
{
    m_style.verticalAlignment.type = style.verticalAlign();
    if (m_style.verticalAlignment.type == VerticalAlign::Length)
        m_style.verticalAlignment.baselineOffset = floatValueForLength(style.verticalAlignLength(), preferredLineHeight());

    auto setAnnotationIfApplicable = [&] {
        // Generic, non-inline box inline-level content (e.g. replaced elements) can't have annotations.
        if (!isRootInlineBox() && !isInlineBox())
            return;
        auto hasTextEmphasis =  style.textEmphasisMark() != TextEmphasisMark::None;
        if (!hasTextEmphasis)
            return;
        auto emphasisPosition = style.textEmphasisPosition();
        // Normally we resolve visual -> logical values at pre-layout time, but emphaisis values are not part of the general box geometry.
        auto hasAboveTextEmphasis = false;
        auto hasUnderTextEmphasis = false;
        if (style.isHorizontalWritingMode()) {
            hasAboveTextEmphasis = emphasisPosition.contains(TextEmphasisPosition::Over);
            hasUnderTextEmphasis = !hasAboveTextEmphasis && emphasisPosition.contains(TextEmphasisPosition::Under);
        } else {
            hasAboveTextEmphasis = emphasisPosition.contains(TextEmphasisPosition::Right) || emphasisPosition == TextEmphasisPosition::Over;  
            hasUnderTextEmphasis = !hasAboveTextEmphasis && (emphasisPosition.contains(TextEmphasisPosition::Left) || emphasisPosition == TextEmphasisPosition::Under);
        }

        if (hasAboveTextEmphasis || hasUnderTextEmphasis) {
            InlineLayoutUnit annotationSize = roundToInt(style.fontCascade().floatEmphasisMarkHeight(style.textEmphasisMarkString()));
            m_annotation = { hasAboveTextEmphasis ? Annotation::Type::Above : Annotation::Type::Under, annotationSize };
        }
    };
    setAnnotationIfApplicable();
}

inline void InlineLevelBox::setHasContent()
{
    ASSERT(isInlineBox());
    m_hasContent = true;
}

inline InlineLayoutUnit InlineLevelBox::preferredLineHeight() const
{
    // FIXME: Remove integral flooring when legacy line layout stops using it.
    if (isPreferredLineHeightFontMetricsBased())
        return primarymetricsOfPrimaryFont().lineSpacing();

    if (m_style.lineHeight.isPercentOrCalculated())
        return floorf(minimumValueForLength(m_style.lineHeight, fontSize()));
    return floorf(m_style.lineHeight.value());
}


inline bool InlineLevelBox::hasLineBoxRelativeAlignment() const
{
    auto verticalAlignment = verticalAlign().type;
    return verticalAlignment == VerticalAlign::Top || verticalAlignment == VerticalAlign::Bottom;
}

inline InlineLevelBox InlineLevelBox::createAtomicInlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth)
{
    return InlineLevelBox { layoutBox, style, logicalLeft, { logicalWidth, { } }, Type::AtomicInlineLevelBox };
}

inline InlineLevelBox InlineLevelBox::createInlineBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox isLineSpanning)
{
    return InlineLevelBox { layoutBox, style, logicalLeft, { logicalWidth, { } }, isLineSpanning == LineSpanningInlineBox::Yes ? Type::LineSpanningInlineBox : Type::InlineBox, { } };
}

inline InlineLevelBox InlineLevelBox::createLineBreakBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft)
{
    return InlineLevelBox { layoutBox, style, logicalLeft, { }, Type::LineBreakBox };
}

inline InlineLevelBox InlineLevelBox::createGenericInlineLevelBox(const Box& layoutBox, const RenderStyle& style, InlineLayoutUnit logicalLeft)
{
    return InlineLevelBox { layoutBox, style, logicalLeft, { }, Type::GenericInlineLevelBox };
}

inline bool InlineLevelBox::lineBoxContain() const
{
    if (isRootInlineBox())
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Block, LineBoxContain::Inline }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::Font, LineBoxContain::Glyphs }));

    if (isAtomicInlineLevelBox())
        return m_style.lineBoxContain.contains(LineBoxContain::Replaced);

    if (isInlineBox()) {
        // Either the inline box itself is included or its text content thorugh Glyph and Font.
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Inline, LineBoxContain::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::Font, LineBoxContain::Glyphs }));
    }

    if (isLineBreakBox())
        return m_style.lineBoxContain.containsAny({ LineBoxContain::Inline, LineBoxContain::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ LineBoxContain::Font, LineBoxContain::Glyphs }));

    return true;
}

}
}

