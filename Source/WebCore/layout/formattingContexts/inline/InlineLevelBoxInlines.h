/**
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "CSSKeywordList.h"
#include "FontCascadeInlines.h"
#include "InlineLevelBox.h"
#include "LayoutBoxInlines.h"
#include "StyleComputedStyle+GettersInlines.h"

namespace WebCore::Layout {

template<typename PreferredLineHeightFunctor> InlineLevelBox::VerticalAlignment toInlineBoxLevelVerticalAlign(const Style::ComputedStyle& style, bool verticalAlignShorthandEnabled, NOESCAPE const PreferredLineHeightFunctor& preferredLineHeightFunctor)
{
    // Inline layout cannot reach a Settings (LayoutState copies the feature flags out of the document), so with the
    // shorthand disabled this reads what effectiveVerticalAlign() returns, the CSS 2 'vertical-align' value.
    if (!verticalAlignShorthandEnabled) [[likely]] {
        return WTF::switchOn(style.verticalAlign(),
            [](CSS::SpecificKeyword auto const& keyword) -> InlineLevelBox::VerticalAlignment {
                return keyword;
            },
            [&](const Style::VerticalAlign::LengthPercentage& value) -> InlineLevelBox::VerticalAlignment {
                return Style::evaluate<InlineLayoutUnit>(value, preferredLineHeightFunctor, style.usedZoomForLength());
            }
        );
    }

    // 'vertical-align' is the CSS Inline 3 shorthand: build its equivalent CSS 2 value from the longhands.
    // https://drafts.csswg.org/css-inline-3/#transverse-alignment
    // The values of 'alignment-baseline' that have a CSS 2 'vertical-align' equivalent. The others, like 'alphabetic', are
    // applied on top of a baseline-relative value (see LineBoxBuilder::setAlignmentBaselineOffsetIfApplicable), when
    // CSSAlignmentBaselineOnInlineBoxesEnabled is enabled.
    auto alignmentBaseline = [&] -> std::optional<InlineLevelBox::VerticalAlignment> {
        switch (style.alignmentBaseline()) {
        case AlignmentBaseline::Middle:
            return CSS::Keyword::Middle { };
        case AlignmentBaseline::TextTop:
        case AlignmentBaseline::TextBeforeEdge: // Legacy SVG alias of 'text-top'.
            return CSS::Keyword::TextTop { };
        case AlignmentBaseline::TextBottom:
        case AlignmentBaseline::TextAfterEdge: // Legacy SVG alias of 'text-bottom'.
            return CSS::Keyword::TextBottom { };
        case AlignmentBaseline::WebkitBaselineMiddle:
            return CSS::Keyword::WebkitBaselineMiddle { };
        default:
            return std::nullopt;
        }
    }();

    // FIXME: CSS 2 can't express a baseline-relative shift together with one of the alignment baselines above, so the shift
    // is dropped. It can't express the 'center' shift either.
    auto alignmentBaselineOr = [&](InlineLevelBox::VerticalAlignment&& shift) {
        return alignmentBaseline ? WTF::move(*alignmentBaseline) : WTF::move(shift);
    };

    return style.baselineShift().switchOn(
        [](const CSS::Keyword::Top& keyword) -> InlineLevelBox::VerticalAlignment {
            return keyword;
        },
        [](const CSS::Keyword::Bottom& keyword) -> InlineLevelBox::VerticalAlignment {
            return keyword;
        },
        [&](const CSS::Keyword::Center&) -> InlineLevelBox::VerticalAlignment {
            return alignmentBaselineOr(CSS::Keyword::Baseline { });
        },
        [&](const CSS::Keyword::Baseline&) -> InlineLevelBox::VerticalAlignment {
            return alignmentBaselineOr(CSS::Keyword::Baseline { });
        },
        [&](const CSS::Keyword::Sub& keyword) -> InlineLevelBox::VerticalAlignment {
            return alignmentBaselineOr(keyword);
        },
        [&](const CSS::Keyword::Super& keyword) -> InlineLevelBox::VerticalAlignment {
            return alignmentBaselineOr(keyword);
        },
        [&](const Style::BaselineShift::LengthPercentage& length) -> InlineLevelBox::VerticalAlignment {
            if (auto fixed = length.tryFixed(); fixed && fixed->isZero())
                return alignmentBaselineOr(CSS::Keyword::Baseline { });
            // Lengths and percentages evaluate exactly like the CSS 2 length path, percentages against the preferred line height.
            return alignmentBaselineOr(Style::evaluate<InlineLayoutUnit>(length, preferredLineHeightFunctor, style.usedZoomForLength()));
        }
    );
}

inline InlineLevelBox::InlineLevelBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutSize logicalSize, Type type, bool verticalAlignShorthandEnabled, EnumSet<PositionWithinLayoutBox> positionWithinLayoutBox)
    : m_layoutBox(layoutBox)
    , m_logicalRect({ }, logicalLeft, logicalSize.width(), logicalSize.height())
    , m_hasContent(layoutBox.isRubyBase() && layoutBox.associatedRubyAnnotationBox()) // Normally we set inline box's has-content state as we come across child content, but ruby annotations are not visible to inline layout.
    , m_isFirstWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::First))
    , m_isLastWithinLayoutBox(positionWithinLayoutBox.contains(PositionWithinLayoutBox::Last))
    , m_type(type)
    , m_style({ protect(style.fontCascade())->metricsOfPrimaryFont(), style.textAutosizingAdjustedLineHeight(), style.textBoxTrim(), style.textBoxEdge(), style.lineFitEdge(), style.usedZoomForLength(), style.lineBoxContain(), InlineLayoutUnit(style.fontCascade().fontDescription().usedSize()), toInlineBoxLevelVerticalAlign(style, verticalAlignShorthandEnabled, [this] { return preferredLineHeight(); }), style.alignmentBaseline() })
{
}

inline InlineLevelBox InlineLevelBox::createAtomicInlineBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, bool verticalAlignShorthandEnabled)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, Type::AtomicInlineBox, verticalAlignShorthandEnabled };
}

inline InlineLevelBox InlineLevelBox::createGenericInlineLevelBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, bool verticalAlignShorthandEnabled)
{
    return { layoutBox, style, logicalLeft, { }, Type::GenericInlineLevelBox, verticalAlignShorthandEnabled };
}

inline InlineLevelBox InlineLevelBox::createInlineBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, LineSpanningInlineBox isLineSpanning, bool verticalAlignShorthandEnabled)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, isLineSpanning == LineSpanningInlineBox::Yes ? Type::LineSpanningInlineBox : Type::InlineBox, verticalAlignShorthandEnabled, { } };
}

inline InlineLevelBox InlineLevelBox::createLineBreakBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, bool verticalAlignShorthandEnabled)
{
    return { layoutBox, style, logicalLeft, { }, Type::LineBreakBox, verticalAlignShorthandEnabled };
}

inline InlineLevelBox InlineLevelBox::createRootInlineBox(const Box& layoutBox, const WebCore::Style::ComputedStyle& style, InlineLayoutUnit logicalLeft, InlineLayoutUnit logicalWidth, bool verticalAlignShorthandEnabled)
{
    return { layoutBox, style, logicalLeft, { logicalWidth, { } }, Type::RootInlineBox, verticalAlignShorthandEnabled, { } };
}

inline bool InlineLevelBox::mayStretchLineBox() const
{
    if (isRootInlineBox())
        return m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::Block, WebCore::Style::WebkitLineBoxContainValue::Inline }) || (hasContent() && m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::InitialLetter, WebCore::Style::WebkitLineBoxContainValue::Font, WebCore::Style::WebkitLineBoxContainValue::Glyphs }));

    if (isAtomicInlineBox())
        return m_style.lineBoxContain.contains(WebCore::Style::WebkitLineBoxContainValue::Replaced);

    if (isInlineBox()) {
        // Either the inline box itself is included or its text content through Glyph and Font.
        return m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::Inline, WebCore::Style::WebkitLineBoxContainValue::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::Font, WebCore::Style::WebkitLineBoxContainValue::Glyphs }));
    }

    if (isLineBreakBox())
        return m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::Inline, WebCore::Style::WebkitLineBoxContainValue::InlineBox }) || (hasContent() && m_style.lineBoxContain.containsAny({ WebCore::Style::WebkitLineBoxContainValue::Font, WebCore::Style::WebkitLineBoxContainValue::Glyphs }));

    return true;
}

inline void InlineLevelBox::setTextEmphasis(std::pair<InlineLayoutUnit, InlineLayoutUnit> textEmphasis)
{
    if (!textEmphasis.first && !textEmphasis.second)
        return;
    if (textEmphasis.first) {
        m_textEmphasis = TextEmphasis { textEmphasis.first, 0.f };
        return;
    }
    m_textEmphasis = TextEmphasis { 0.f, textEmphasis.second };
}

}
