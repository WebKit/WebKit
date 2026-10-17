/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/LayoutUnit.h>
#include <WebCore/WritingMode.h>
#include <optional>
#include <wtf/CheckedRef.h>

namespace WebCore {

class RenderBox;

namespace Style {
class ComputedStyle;
}

namespace Layout {

// The absolute positioning layout model (css-position-3 §4) for a box in a containing block with a horizontal writing mode: the
// inset-modified containing block, the size, the auto margins and the alignment of the margin box (normal is start alignment).
// The inline axis first; the block axis may need the content laid out with the inline size.
class OutOfFlowGeometry {
public:
    // The padding box of the containing block (CSS 2.2 §10.1).
    struct ContainingBlock {
        LayoutUnit inlineSize;
        LayoutUnit blockSize;
        WritingMode writingMode;
    };
    // Where the margin box would start with position: static (CSS 2.2 §10.3.7, §10.6.4), relative to the padding box of the containing
    // block, in its direction. The inline position is where the margin box would end when the static-position containing block has the
    // opposite direction (css-position-3 §3.1).
    struct StaticPosition {
        LayoutUnit inlinePosition;
        LayoutUnit blockPosition;
        bool inlinePositionIsMarginBoxEnd { false };
    };
    // The used values in one axis, the border box start relative to the padding box of the containing block.
    struct AxisGeometry {
        LayoutUnit borderBoxSize;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        LayoutUnit borderBoxStart;
    };

    OutOfFlowGeometry(const RenderBox&, const ContainingBlock&, const StaticPosition&, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding);

    // FIXME: An auto inline size with an auto inset is the fit-content size; coverage keeps those out, and this uses the stretch-fit size.
    AxisGeometry computeInlineAxisGeometry() const;
    // The content box block size when it does not depend on the content, which percentages inside the box resolve against.
    std::optional<LayoutUnit> definiteContentBoxBlockSize() const;
    AxisGeometry computeBlockAxisGeometry(LayoutUnit intrinsicContentBlockSize) const;

private:
    enum class Axis : bool { Inline, Block };
    struct AxisValues {
        std::optional<LayoutUnit> insetStart;
        std::optional<LayoutUnit> insetEnd;
        std::optional<LayoutUnit> marginStart;
        std::optional<LayoutUnit> marginEnd;
        LayoutUnit containingBlockSize;
        LayoutUnit borderAndPadding;
        LayoutUnit staticPosition;
        bool staticPositionIsMarginBoxEnd { false };
    };
    static AxisGeometry computeAxisGeometry(Axis, const AxisValues&, LayoutUnit borderBoxSize);
    AxisValues inlineAxisValues() const;
    AxisValues blockAxisValues() const;
    std::optional<LayoutUnit> definiteBorderBoxBlockSize(const AxisValues&) const;
    template<typename SizeType> LayoutUnit borderBoxSizeFor(const SizeType&, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const;
    template<typename MinimumSizeType, typename MaximumSizeType> LayoutUnit clamp(LayoutUnit borderBoxSize, const MinimumSizeType&, const MaximumSizeType&, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const;
    const Style::ComputedStyle& style() const;

    const CheckedRef<const RenderBox> m_box;
    const ContainingBlock m_containingBlock;
    const StaticPosition m_staticPosition;
    const LayoutUnit m_horizontalBorderAndPadding;
    const LayoutUnit m_verticalBorderAndPadding;
};

}
}
