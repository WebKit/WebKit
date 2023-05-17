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
#include "LengthFunctions.h"
#include "StyleTextBoxEdge.h"
#include <wtf/OptionSet.h>

namespace WebCore {
namespace Layout {

class LineBox;
class LineBoxBuilder;
class LineBoxVerticalAligner;

class InlineLevelBox {
public:
    enum class LineSpanningInlineBox : bool { No, Yes };
    static inline InlineLevelBox createInlineBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox = LineSpanningInlineBox::No);
    static inline InlineLevelBox createRootInlineBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
    static inline InlineLevelBox createAtomicInlineLevelBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth);
    static inline InlineLevelBox createLineBreakBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft);
    static inline InlineLevelBox createGenericInlineLevelBox(const Box&, const RenderStyle&, InlineLayoutUnit logicalLeft);

    struct AscentAndDescent {
        InlineLayoutUnit ascent { 0 };
        InlineLayoutUnit descent { 0 };

        InlineLayoutUnit height() const { return ascent + descent; }
        bool operator==(const AscentAndDescent& other) const { return ascent == other.ascent && descent == other.descent; }
        // FIXME: Remove this.
        // We need floor/ceil to match legacy layout integral positioning.
        void round();
    };
    InlineLayoutUnit ascent() const { return m_ascentAndDescent.ascent; }
    InlineLayoutUnit descent() const { return m_ascentAndDescent.descent; }
    // See https://www.w3.org/TR/css-inline-3/#layout-bounds
    AscentAndDescent layoutBounds() const { return m_layoutBounds; }

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

    inline bool mayStretchLineBox() const;

    const FontMetrics& primarymetricsOfPrimaryFont() const { return m_style.primaryFontMetrics; }
    InlineLayoutUnit fontSize() const { return m_style.primaryFontSize; }

    // FIXME: Maybe it's time to subclass inline box types.
    TextBoxEdge textBoxEdge() const { return m_style.textBoxEdge; }
    TextBoxTrim textBoxTrim() const { return m_style.textBoxTrim; }
    InlineLayoutUnit inlineBoxContentOffsetForTextBoxTrim() const { return m_inlineBoxContentOffsetForTextBoxTrim; }

    bool hasAnnotation() const { return (hasContent() || isAtomicInlineLevelBox()) && m_annotation.has_value(); };
    std::optional<InlineLayoutUnit> annotationAbove() const { return hasAnnotation() ? std::optional { m_annotation->above } : std::nullopt; }
    std::optional<InlineLayoutUnit> annotationBelow() const { return hasAnnotation() ? std::optional { m_annotation->below } : std::nullopt; }

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
    InlineLayoutUnit logicalRight() const { return m_logicalRect.right(); }
    InlineLayoutUnit logicalWidth() const { return m_logicalRect.width(); }
    InlineLayoutUnit logicalHeight() const { return m_logicalRect.height(); }

    // FIXME: Remove legacy rounding.
    void setLogicalWidth(InlineLayoutUnit logicalWidth) { m_logicalRect.setWidth(logicalWidth); }
    void setLogicalHeight(InlineLayoutUnit logicalHeight) { m_logicalRect.setHeight(roundToInt(logicalHeight)); }
    void setLogicalTop(InlineLayoutUnit logicalTop) { m_logicalRect.setTop(logicalTop >= 0 ? roundToInt(logicalTop) : -roundToInt(-logicalTop)); }
    void setLogicalLeft(InlineLayoutUnit logicalLeft) { m_logicalRect.setLeft(logicalLeft); }
    void setAscentAndDescent(AscentAndDescent ascentAndDescent) { m_ascentAndDescent = { InlineLayoutUnit(roundToInt(ascentAndDescent.ascent)), InlineLayoutUnit(roundToInt(ascentAndDescent.descent)) }; }
    void setLayoutBounds(const AscentAndDescent& layoutBounds) { m_layoutBounds = { InlineLayoutUnit(roundToInt(layoutBounds.ascent)), InlineLayoutUnit(roundToInt(layoutBounds.descent)) }; }
    void setInlineBoxContentOffsetForTextBoxTrim(InlineLayoutUnit offset) { m_inlineBoxContentOffsetForTextBoxTrim = offset; }

    void setIsFirstBox() { m_isFirstWithinLayoutBox = true; }
    void setIsLastBox() { m_isLastWithinLayoutBox = true; }

private:
    CheckedRef<const Box> m_layoutBox;
    // This is the combination of margin and border boxes. Inline level boxes are vertically aligned using their margin boxes.
    InlineRect m_logicalRect;
    AscentAndDescent m_layoutBounds;
    AscentAndDescent m_ascentAndDescent;
    InlineLayoutUnit m_inlineBoxContentOffsetForTextBoxTrim { 0.f };
    bool m_hasContent { false };
    // These bits are about whether this inline level box is the first/last generated box of the associated Layout::Box
    // (e.g. always true for atomic inline level boxes, but inline boxes spanning over multiple lines can produce separate first/last boxes).
    bool m_isFirstWithinLayoutBox { false };
    bool m_isLastWithinLayoutBox { false };
    Type m_type { Type::InlineBox };

    struct Style {
        const FontMetrics& primaryFontMetrics;
        const Length& lineHeight;
        TextBoxEdge textBoxEdge;
        TextBoxTrim textBoxTrim;
        WTF::OptionSet<LineBoxContain> lineBoxContain;
        InlineLayoutUnit primaryFontSize { 0 };
        VerticalAlignment verticalAlignment { };
    };
    Style m_style;

    struct Annotation {
        InlineLayoutUnit above;
        InlineLayoutUnit below;
    };
    std::optional<Annotation> m_annotation;
};

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

inline void InlineLevelBox::AscentAndDescent::round()
{
    ascent = floorf(ascent);
    descent = ceilf(descent);
}

}
}

