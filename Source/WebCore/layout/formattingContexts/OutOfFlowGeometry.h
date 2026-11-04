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

#include "FormattingConstraints.h"
#include "StyleSelfAlignmentData.h"
#include <WebCore/LayoutRect.h>
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

// The absolute positioning layout model (css-position-3 §4) for a box in the logical coordinates of its containing block, whose writing mode
// is the box's (not orthogonal): the inset-modified containing block, the size, the auto margins and the alignment of the margin box. The
// inline axis first; the block axis may need the content laid out with the inline size.
class OutOfFlowGeometry {
public:
    // The padding box of the containing block (CSS 2.2 §10.1). A scroll container's scrollable overflow reaches past its end sides only
    // (css-overflow-3 §2.2), and its scrollable containing block, relative to its padding box, is what position-area divides
    // (css-position-4 §2.2, css-anchor-position-1 §2.1).
    struct ContainingBlock {
        LayoutUnit inlineSize;
        LayoutUnit blockSize;
        WritingMode writingMode;
        bool isScrollContainer { false };
        std::optional<LayoutRect> scrollableContainingBlock { };
    };
    // Where the margin box would start with position: static (CSS 2.2 §10.3.7, §10.6.4), relative to the padding box of the containing
    // block, in its direction. The inline position is where the margin box would end when the static-position containing block has the
    // opposite direction (css-position-3 §3.1).
    struct StaticPosition {
        // A flex container aligns the margin box in the static-position rectangle that starts at the static position, its content box, as if
        // the box were its only flex item (css-flexbox-1 §4.1), in the direction of the static-position containing block. Safe alignment
        // keeps it at the start when it overflows (css-align-3 §4.4).
        struct Alignment {
            enum class Position : uint8_t { Start, Center, End };
            LayoutUnit rectangleSize;
            Position position { Position::Start };
            bool isSafe { false };
        };
        LayoutUnit inlinePosition;
        LayoutUnit blockPosition;
        bool inlinePositionIsMarginBoxEnd { false };
        std::optional<Alignment> inlineAlignment { };
        std::optional<Alignment> blockAlignment { };
    };
    // The border box of the default anchor (css-anchor-position-1 §2.2) of a box that position-area or anchor-center positions against it,
    // relative to the padding box of the containing block.
    struct DefaultAnchor {
        LayoutUnit inlineStart;
        LayoutUnit inlineSize;
        LayoutUnit blockStart;
        LayoutUnit blockSize;
    };
    // The used values in one axis, the border box start relative to the padding box of the containing block.
    struct AxisGeometry {
        LayoutUnit borderBoxSize;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        LayoutUnit borderBoxStart;
    };

    OutOfFlowGeometry(const RenderBox&, const ContainingBlock&, const StaticPosition&, const std::optional<DefaultAnchor>&, LayoutUnit inlineAxisBorderAndPadding, LayoutUnit blockAxisBorderAndPadding);

    // An auto inline size is the fit-content size when an inset in the inline axis is auto or justify-self is neither normal nor stretch
    // (css-position-3 §4.3), which takes the min-content and max-content inline sizes of the content (css-sizing-3 §5.1). So does the
    // automatic minimum inline size of a box whose preferred aspect ratio gives its inline size (css-sizing-4 §5.1).
    bool needsContentInlineSizes() const;
    AxisGeometry computeInlineAxisGeometry(std::optional<IntrinsicWidthConstraints> contentInlineSizes = { }) const;
    // The content box block size when it does not depend on the content, which percentages inside the box resolve against; with a
    // preferred aspect ratio, from the border box inline size.
    std::optional<LayoutUnit> definiteContentBoxBlockSize(std::optional<LayoutUnit> borderBoxInlineSize = { }) const;
    AxisGeometry computeBlockAxisGeometry(LayoutUnit intrinsicContentBlockSize, LayoutUnit borderBoxInlineSize) const;
    // A replaced element's own formatting context decides its border box size, from its natural size and aspect ratio (CSS 2.2 §10.3.8,
    // §10.6.5); this places that border box.
    AxisGeometry computeReplacedInlineAxisGeometry(LayoutUnit borderBoxSize) const;
    AxisGeometry computeReplacedBlockAxisGeometry(LayoutUnit borderBoxSize) const;

private:
    enum class Axis : bool { Inline, Block };
    // The axis whose automatic size a preferred aspect ratio gives from the size in the other axis (css-position-3 §4.3).
    std::optional<Axis> ratioDependentAxis() const;
    bool hasFitContentInlineSize() const;
    struct AxisValues {
        std::optional<LayoutUnit> insetStart;
        std::optional<LayoutUnit> insetEnd;
        std::optional<LayoutUnit> marginStart;
        std::optional<LayoutUnit> marginEnd;
        // The range of the containing block in the axis, which percentages in the axis resolve against.
        LayoutUnit containingBlockStart;
        LayoutUnit containingBlockSize;
        LayoutUnit borderAndPadding;
        LayoutUnit staticPosition;
        bool staticPositionIsMarginBoxEnd { false };
        std::optional<StaticPosition::Alignment> staticPositionAlignment;
        StyleSelfAlignmentData selfAlignment;
        // The range of the default anchor, when the box is positioned against it in the axis.
        std::optional<std::pair<LayoutUnit, LayoutUnit>> defaultAnchor;
    };
    static std::pair<LayoutUnit, LayoutUnit> insetModifiedContainingBlock(const AxisValues&);
    AxisGeometry computeAxisGeometry(Axis, const AxisValues&, LayoutUnit borderBoxSize) const;
    StyleSelfAlignmentData selfAlignment(Axis, bool usesStaticPosition) const;
    bool alignmentAppliesStretch(Axis) const;
    LayoutUnit alignmentOffset(Axis, const AxisValues&, LayoutUnit rangeStart, LayoutUnit rangeEnd, LayoutUnit freeSpace) const;
    std::optional<std::pair<LayoutUnit, LayoutUnit>> defaultAnchorRange(Axis) const;
    std::pair<LayoutUnit, LayoutUnit> containingBlockRange(Axis) const;
    AxisValues axisValues(Axis) const;
    std::optional<LayoutUnit> definiteBorderBoxBlockSize(const AxisValues&, std::optional<LayoutUnit> borderBoxInlineSize = { }) const;
    template<typename SizeType> LayoutUnit borderBoxSizeFor(const SizeType&, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const;
    template<typename MinimumSizeType, typename MaximumSizeType> LayoutUnit clamp(LayoutUnit borderBoxSize, const MinimumSizeType&, const MaximumSizeType&, LayoutUnit containingBlockSize, LayoutUnit borderAndPadding) const;
    const Style::ComputedStyle& style() const;

    const CheckedRef<const RenderBox> m_box;
    const ContainingBlock m_containingBlock;
    const StaticPosition m_staticPosition;
    const std::optional<DefaultAnchor> m_defaultAnchor;
    const LayoutUnit m_inlineAxisBorderAndPadding;
    const LayoutUnit m_blockAxisBorderAndPadding;
};

}
}
