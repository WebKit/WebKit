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

class RenderBox;

namespace Style {
class ComputedStyle;
class UnevaluatedCalcSize;
struct PreferredSize;
}

namespace Layout {

class BoxGeometry;

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
    static InlineSizeAndMargins inlineSizeAndMargins(const RenderBox&, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<IntrinsicWidthConstraints> contentInlineSizes, std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits = { }, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize = { });
    // The border box inline size a preferred aspect ratio gives an auto inline size, from a definite block size within the min and max block
    // sizes (css-sizing-4 §5.1, like RenderBox::computeLogicalWidthFromAspectRatio). None without a ratio or a definite block size.
    static std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize(const RenderBox&, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);
    // The automatic minimum inline size of a box whose preferred aspect ratio gives its inline size is its min-content size, capped by its
    // max inline size, unless it is a scroll container (css-sizing-4 §5.1, like RenderBox::constrainLogicalWidthByMinMax).
    static bool hasAspectRatioAutomaticMinimumInlineSize(const RenderBox&, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize, WritingMode);
    // The min and max border box inline sizes a preferred aspect ratio transfers from the min and max block sizes (css-sizing-4 §5.1, like
    // RenderBox::computeMinMaxLogicalWidthFromAspectRatio). None without a ratio.
    static std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits(const RenderBox&, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);
    // The start and end margins of a box whose border box inline size is already known, from the same §10.3.3 equation.
    static std::pair<LayoutUnit, LayoutUnit> inlineMargins(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle);
    // A block-level box with a justify-self other than normal, stretch or legacy is aligned in the inline axis of its containing block, which
    // sizes an auto inline size to fit its content and keeps its margins as they are (css-align-3 §6.1).
    static bool isAlignedByJustifySelf(const RenderBox&, const Style::ComputedStyle& containingBlockStyle);
    // How far justify-self moves the margin box from the inline start of the containing block, unless an auto margin aligns it.
    static LayoutUnit justifySelfOffset(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit marginStart, LayoutUnit marginEnd, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle);
    // Its inline size, min inline size or max inline size is min-content, max-content or fit-content.
    static bool hasIntrinsicInlineSize(const RenderBox&, WritingMode);
    // The border box size of a calc-size(): its calculation, with the size of its basis standing for `size` and percentages resolving against
    // percentageBasis (css-values-5 §11.1, like RenderBox::resolveCalcSizeContentSize). Box-sizing decides whether `size` and the result
    // are border box or content box sizes.
    static LayoutUnit calcSizeBorderBoxSize(const Style::UnevaluatedCalcSize&, const Style::ComputedStyle&, LayoutUnit basisBorderBoxSize, LayoutUnit borderAndPadding, LayoutUnit percentageBasis);
    // CSS 2.2 §10.6.3 (block-level, non-replaced elements in normal flow) with min/max-height (§10.7), and the stretch-fit block size for
    // stretch (css-sizing-4 §6.1).
    static LayoutUnit borderBoxBlockSize(const RenderBox&, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);
    // The border box block size a preferred aspect ratio gives an auto block size, from the inline size and within the min and max block sizes
    // (css-sizing-4 §5.1, like RenderBox::blockSizeFromAspectRatio), which is definite. With the block size of the content, an auto min block
    // size is the automatic minimum block size: the content's, capped by the max block size, unless the box is a scroll container. None without
    // a ratio or with a block size.
    static std::optional<LayoutUnit> aspectRatioBorderBoxBlockSize(const RenderBox&, LayoutUnit borderBoxInlineSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode, std::optional<LayoutUnit> intrinsicContentBlockSize = { });
    // Its block-size, min-block-size or max-block-size is a percentage of the containing block's block size, has one in a calc-size(), or
    // stretches to it.
    static bool hasPercentageBlockSize(const RenderBox&, WritingMode);
    // Its block-size, min-block-size or max-block-size is -webkit-fill-available.
    static bool hasFillAvailableBlockSize(const RenderBox&, WritingMode);
    // The block size -webkit-fill-available block sizes of the children of a block container resolve to (RenderBox::availableLogicalHeight):
    // its definite content box block size, or else the one of its containing block without its margins, border and padding, within its min
    // and max block sizes.
    static LayoutUnit fillAvailableBlockSize(const RenderBox& blockContainer, const BoxGeometry&, std::optional<LayoutUnit> contentBoxBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, LayoutUnit containingBlockFillAvailableBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);
    // The block size of the content of an empty editable block container: the line height of its first line, for the caret (RenderBlock::hasLineIfEmpty,
    // RenderBox::lineHeight).
    static std::optional<LayoutUnit> emptyEditableContentBlockSize(const RenderBox& blockContainer);
    // The min-content, max-content and fit-content block sizes of a block container are its automatic block size (css-sizing-3 §2.1, §3.2), and a
    // percentage block size behaves as auto when the block size of the containing block is not definite (CSS 2.2 §10.5).
    static bool blockSizeBehavesAsAuto(const Style::PreferredSize& blockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize);
    static std::optional<LayoutUnit> definiteContentBoxBlockSize(const RenderBox&, const BoxGeometry&, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode);
};

}
}
