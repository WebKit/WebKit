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

// The absolute positioning layout model (css-position-3 §4) for a box in the logical coordinates of its containing block: the inset-modified
// containing block, the size, the auto margins and the alignment of the margin box. The insets, the margins and the self-alignment are in the
// writing mode of the containing block, the sizes in the box's own, whose inline axis is the block axis of the containing block in an
// orthogonal flow (css-writing-modes-4 §7.3). The box's inline axis first; its block axis may need the content laid out with the inline size.
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
        // The border box block size -webkit-fill-available gives the box: the available block size of the containing block
        // (RenderBox::availableLogicalHeight), not the size of the inset-modified containing block.
        std::optional<LayoutUnit> fillAvailableBlockSize { };
        // Where a fixed positioned box with a default anchor that overflows the layout viewport, its containing block in the view, can stay
        // instead, relative to it: as far as scrolling the view reaches (PositionedLayoutConstraints::resolveAlignmentShift).
        std::optional<LayoutRect> fixedPositionOverflowArea { };
    };
    // Where the margin box would start with position: static (CSS 2.2 §10.3.7, §10.6.4), relative to the padding box of the containing
    // block, in its direction. It is where the margin box would end in an axis where the static-position containing block's start is the
    // end of the containing block, with the opposite direction or another writing mode (css-position-3 §3.1).
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
        bool blockPositionIsMarginBoxEnd { false };
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
    // The used values in one axis of the containing block, the border box start relative to its padding box.
    struct AxisGeometry {
        LayoutUnit borderBoxSize;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
        LayoutUnit borderBoxStart;
    };

    // The border and padding are the box's in its own inline and block axes.
    OutOfFlowGeometry(const RenderBox&, const ContainingBlock&, const StaticPosition&, const std::optional<DefaultAnchor>&, LayoutUnit inlineAxisBorderAndPadding, LayoutUnit blockAxisBorderAndPadding);
    // Whether the box's inline axis is the block axis of the containing block, where computeInlineAxisGeometry places it.
    bool isOrthogonal() const { return m_isOrthogonal; }

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
    // The axis of the containing block that is the box's inline or block axis.
    Axis containingBlockAxis(Axis) const;
    bool hasAutoInset(Axis containingBlockAxis) const;
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
    // The border box sizes the sizing keywords resolve to in an axis (css-sizing-3 §3.2, css-sizing-4 §6.1): min-content and max-content
    // from the inline sizes of the content in the inline axis and the block size of the content in the block axis; fit-content, the
    // stretch-fit size between the two; stretch, the stretch-fit size; and -webkit-fill-available, which is stretch in the inline axis.
    struct KeywordSizes {
        std::optional<LayoutUnit> minContent;
        std::optional<LayoutUnit> maxContent;
        LayoutUnit stretchFit;
        std::optional<LayoutUnit> fillAvailable;
    };
    KeywordSizes keywordSizes(const AxisValues&, std::optional<LayoutUnit> minContent, std::optional<LayoutUnit> maxContent, std::optional<LayoutUnit> fillAvailable) const;
    // In the box's block axis, which is the containing block's block axis unless the flow is orthogonal.
    std::optional<LayoutUnit> fillAvailableBlockSize() const { return m_isOrthogonal ? std::nullopt : m_containingBlock.fillAvailableBlockSize; }
    static std::optional<LayoutUnit> fitContentSize(const KeywordSizes&);
    template<typename SizeType> std::optional<LayoutUnit> borderBoxSizeFor(const SizeType&, const AxisValues&, const KeywordSizes&) const;
    template<typename MinimumSizeType, typename MaximumSizeType> LayoutUnit clamp(LayoutUnit borderBoxSize, const MinimumSizeType&, const MaximumSizeType&, const AxisValues&, const KeywordSizes&) const;
    const Style::ComputedStyle& style() const;

    const CheckedRef<const RenderBox> m_box;
    const ContainingBlock m_containingBlock;
    const StaticPosition m_staticPosition;
    const std::optional<DefaultAnchor> m_defaultAnchor;
    const LayoutUnit m_inlineAxisBorderAndPadding;
    const LayoutUnit m_blockAxisBorderAndPadding;
    const bool m_isOrthogonal;
};

}
}
