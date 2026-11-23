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
#include <WebCore/LayoutUnit.h>
#include <WebCore/WritingMode.h>
#include <optional>

namespace WebCore {

class RenderBlockFlow;
class RenderBox;

enum class AvailableLogicalHeightType : bool;

namespace Style {
class ComputedStyle;
class UnevaluatedCalcSize;
struct PreferredSize;
}

namespace Layout {

class BoxGeometry;

// The border box block sizes the stretch keyword resolves to, the stretch-fit size (css-sizing-4 §6.1), and -webkit-fill-available, which
// resolves to what it did before the stretch keyword: the available block size, margins included
// (RenderBox::computeSizingKeywordLogicalContentHeightUsing).
struct StretchFitBorderBoxBlockSizes {
    template<typename SizeType>
    std::optional<LayoutUnit> forSize(const SizeType& size) const { return size.isFillAvailable() ? fillAvailable : stretch; }

    std::optional<LayoutUnit> stretch;
    std::optional<LayoutUnit> fillAvailable;
    // The block margins a stretch(<length-percentage>) takes off its argument, whole, whether or not they collapse (css-sizing-4 §3.1).
    LayoutUnit stretchFunctionMargins;
};

class BlockFormattingUtils {
public:
    struct InlineSizeAndMargins {
        LayoutUnit borderBoxInlineSize;
        LayoutUnit marginStart;
        LayoutUnit marginEnd;
    };
    // CSS 2.2 §10.3.3 (block-level, non-replaced elements in normal flow) with min/max-width (§10.4). Auto margins resolve the
    // way RenderBox::computeInlineDirectionMargins resolves them.
    // The min-content, max-content and fit-content inline sizes resolve against the inline sizes of the content (css-sizing-3 §3.2).
    static InlineSizeAndMargins inlineSizeAndMargins(const RenderBox&, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<IntrinsicWidthConstraints> contentInlineSizes, std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits = { }, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize = { }, std::optional<LayoutUnit> availableInlineSize = { });
    // The border box inline size a preferred aspect ratio gives an auto inline size, from a definite block size within the min and max block
    // sizes (css-sizing-4 §5.1, like RenderBox::computeLogicalWidthFromAspectRatio). None without a ratio or a definite block size.
    static std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize(const RenderBox&, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode);
    // The border box inline size the preferred aspect ratio of a box gives a border box block size. None without a ratio.
    static std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize(const RenderBox&, LayoutUnit borderBoxBlockSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding);
    // The automatic minimum inline size of a box whose preferred aspect ratio gives its inline size is its min-content size, capped by its
    // max inline size, unless it is a scroll container (css-sizing-4 §5.1, like RenderBox::constrainLogicalWidthByMinMax).
    static bool hasAspectRatioAutomaticMinimumInlineSize(const RenderBox&, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize, WritingMode);
    static IntrinsicWidthConstraints aspectRatioContentInlineSizes(const RenderBox&, LayoutUnit aspectRatioContentBoxInlineSize, IntrinsicWidthConstraints contentSizes, LayoutUnit borderAndPaddingInlineSize, WritingMode);
    // The min and max border box inline sizes a preferred aspect ratio transfers from the min and max block sizes (css-sizing-4 §5.1, like
    // RenderBox::computeMinMaxLogicalWidthFromAspectRatio). None without a ratio.
    static std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits(const RenderBox&, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode);
    // The start and end margins of a box whose border box inline size is already known, from the same §10.3.3 equation.
    static std::pair<LayoutUnit, LayoutUnit> inlineMargins(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<LayoutUnit> availableInlineSize = { });
    static bool ignoresEndMarginWhenOverConstrained(const RenderBox&, const Style::ComputedStyle& containingBlockStyle);
    // A block-level box with a justify-self other than normal, stretch or legacy is aligned in the inline axis of its containing block, which
    // sizes an auto inline size to fit its content and keeps its margins as they are (css-align-3 §6.1).
    static bool isAlignedByJustifySelf(const RenderBox&, const Style::ComputedStyle& containingBlockStyle);
    // An auto inline size fits the content of a float (CSS 2.2 §10.3.5), of a box justify-self aligns, of a child of a horizontal marquee, and of
    // a form control (RenderBox::sizesLogicalWidthToFitContent). A legend that is not the rendered legend of a fieldset, which the fieldset sizes
    // itself, is a block container that fills its containing block (CSS 2.2 §10.3.3), where the render tree fits it to its content too.
    static bool autoInlineSizeFitsContent(const RenderBox&, const Style::ComputedStyle& containingBlockStyle);
    static bool isScrollingHorizontalMarquee(const RenderBox&);
    // A text control, list box, progress bar, meter or file input: a block container whose renderer sizes it and lays out its content its own
    // way (RenderTextControlSingleLine::layout, RenderListBox::computeLogicalHeight), so its own formatting context lays it out, like a flex
    // container. So does a checkbox or a radio button with display: block, which avoids floats (RenderBox::avoidsFloats).
    static bool isFormControlBlockContainer(const RenderBox&);
    // A block-level box that is not a block container, like a table or a flex container, a block container that establishes a block formatting
    // context for its content (CSS 2.2 §9.4.1), a form control block container, or a column set, which its own layout sizes.
    static bool establishesIndependentFormattingContext(const RenderBox&);
    // How far justify-self moves the margin box from the inline start of the containing block, unless an auto margin aligns it.
    static LayoutUnit justifySelfOffset(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit marginStart, LayoutUnit marginEnd, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle);
    // How far align-content moves the content of a block container, as a unit, in a content box that has this much more block size than the
    // content needs (css-align-3 §5.1), safe unless it says otherwise or the block container scrolls in the block axis. A table cell aligns its
    // content with its intrinsic padding instead (RenderTableCell::computeIntrinsicPadding), and an input does not align it.
    static LayoutUnit alignContentShift(const RenderBox& blockContainer, LayoutUnit freeSpace);
    // Its inline size, min inline size or max inline size is min-content, max-content, fit-content or the legacy min-intrinsic keyword, or
    // its inline size is the legacy intrinsic keyword.
    static bool hasIntrinsicInlineSize(const RenderBox&, WritingMode);
    // The border box size of a calc-size(): its calculation, with the size of its basis standing for `size` and percentages resolving against
    // percentageBasis (css-values-5 §11.1, like RenderBox::resolveCalcSizeContentSize). Box-sizing decides whether `size` and the result
    // are border box or content box sizes.
    static LayoutUnit calcSizeBorderBoxSize(const Style::UnevaluatedCalcSize&, const Style::ComputedStyle&, LayoutUnit basisBorderBoxSize, LayoutUnit borderAndPadding, LayoutUnit percentageBasis);
    // A table cell gives the percentage and calc() block sizes of its children their border box, and percentages that look through one of its
    // children to it resolve as auto (RenderBox::computePercentageLogicalHeight).
    enum class ContainingBlockIsTableCell : bool { No, Yes };
    // CSS 2.2 §10.6.3 (block-level, non-replaced elements in normal flow) with min/max-height (§10.7), and the stretch-fit block size for
    // stretch (css-sizing-4 §6.1).
    static LayoutUnit borderBoxBlockSize(const RenderBox&, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode, ContainingBlockIsTableCell = ContainingBlockIsTableCell::No);
    // The border box block size a preferred aspect ratio gives an auto block size, from the inline size and within the min and max block sizes
    // (css-sizing-4 §5.1, like RenderBox::blockSizeFromAspectRatio), which is definite. With the block size of the content, an auto min block
    // size is the automatic minimum block size: the content's, capped by the max block size, unless the box is a scroll container. None without
    // a ratio or with a block size.
    static std::optional<LayoutUnit> aspectRatioBorderBoxBlockSize(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode, std::optional<LayoutUnit> intrinsicContentBlockSize = { });
    // Its block-size, min-block-size or max-block-size is a percentage of the containing block's block size, has one in a calc-size(), or
    // stretches to it.
    static bool hasPercentageBlockSize(const RenderBox&, WritingMode);
    // A different end page value of the in-flow sibling before a block-level box than the box's start page value forces a page break between
    // them (css-page-3, Using named pages), like RenderBlockFlow::applyBeforeBreak.
    static bool pageValuesForceBreakBefore(const RenderBlockFlow& parent, const RenderBox& child);
    static bool hasOnlyStretchBlockSizes(const RenderBox&);
    static std::optional<LayoutUnit> stretchFitBasis(const RenderBox& containingBlock, std::optional<LayoutUnit> contentBoxBlockSize);
    // Its block-size, min-block-size or max-block-size is -webkit-fill-available.
    static bool hasFillAvailableBlockSize(const RenderBox&, WritingMode);
    // The available block size of a block container (RenderBox::availableLogicalHeight): -webkit-fill-available block sizes of its children
    // resolve to the one without margins, border and padding, and in quirks mode percentage block sizes of its replaced children resolve
    // against the one with them (RenderReplaced::computeReplacedLogicalHeightUsing). It is its definite content box block size, or else the
    // one of its containing block, without its margins, border and padding for the first, within its min and max block sizes.
    static LayoutUnit availableBlockSize(const RenderBox& blockContainer, const BoxGeometry&, std::optional<LayoutUnit> contentBoxBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, LayoutUnit containingBlockAvailableBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode, AvailableLogicalHeightType);
    // The block size of the content of an empty editable block container: the line height of its first line, for the caret (RenderBlock::hasLineIfEmpty,
    // RenderBox::lineHeight).
    static std::optional<LayoutUnit> emptyEditableContentBlockSize(const RenderBox& blockContainer);
    // The min-content, max-content and fit-content block sizes of a block container are its automatic block size (css-sizing-3 §2.1, §3.2), and a
    // percentage block size behaves as auto when the block size of the containing block is not definite (CSS 2.2 §10.5).
    static bool blockSizeBehavesAsAuto(const Style::PreferredSize& blockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize);
    static std::optional<LayoutUnit> definiteContentBoxBlockSize(const RenderBox&, const BoxGeometry&, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes&, WritingMode, ContainingBlockIsTableCell = ContainingBlockIsTableCell::No);
};

}
}
