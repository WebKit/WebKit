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

#include "config.h"
#include "BlockFormattingUtils.h"

#include "LayoutBoxGeometry.h"
#include "RenderBlock.h"
#include "RenderElementInlines.h"
#include "RenderObjectInlines.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "StylePrimitiveNumericTypes+EvaluationMinimum.h"
#include "StyleUnevaluatedCalcSize.h"

namespace WebCore {
namespace Layout {

LayoutUnit BlockFormattingUtils::calcSizeBorderBoxSize(const Style::UnevaluatedCalcSize& calcSize, const Style::ComputedStyle& style, LayoutUnit basisBorderBoxSize, LayoutUnit borderAndPadding, LayoutUnit percentageBasis)
{
    auto zoom = style.usedZoomForLength();
    if (style.boxSizing() == BoxSizing::BorderBox)
        return std::max(borderAndPadding, LayoutUnit::fromFloatRound(calcSize.evaluate(percentageBasis, zoom, basisBorderBoxSize)));
    return LayoutUnit::fromFloatRound(calcSize.evaluate(percentageBasis, zoom, std::max(0_lu, basisBorderBoxSize - borderAndPadding))) + borderAndPadding;
}

// A percentage of the block size of the containing block, which behaves as auto when that is indefinite (CSS 2.2 §10.5). Of a calc-size(), only
// percentages in the basis do; those in the calculation resolve against zero then (css-values-5 §11.1).
template<typename SizeType>
static bool isPercentageOfContainingBlock(const SizeType& size)
{
    if (size.isCalcSize())
        return size.isSpecified() && size.calcSize().basisHasPercentage();
    return size.isPercentOrCalculated();
}

template<typename SizeType>
static std::optional<LayoutUnit> borderBoxBlockSizeFor(const SizeType& size, const Style::ComputedStyle& style, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, std::optional<LayoutUnit> intrinsicContentBlockSize = { })
{
    auto keywordBorderBoxBlockSize = [&] -> std::optional<LayoutUnit> {
        // Without a definite stretch-fit block size, stretch behaves as the initial value (css-sizing-4 §6.1).
        if (size.isStretch()) {
            return stretchFitBorderBoxBlockSize.transform([&](auto stretchFitSize) {
                return std::max(stretchFitSize, borderAndPaddingBlockSize);
            });
        }
        // The intrinsic sizing keywords are the block size of the content, once it is laid out (css-sizing-3 §3.2), and so is auto, but for
        // the min block size, where it is zero.
        if constexpr (std::same_as<SizeType, Style::MinimumSize>) {
            if (size.isAuto())
                return borderAndPaddingBlockSize;
        }
        return intrinsicContentBlockSize.transform([&](auto contentBlockSize) {
            return contentBlockSize + borderAndPaddingBlockSize;
        });
    };
    // A calc-size() with a keyword basis resolves its calculation with the size of the keyword (css-values-5 §11.1).
    if (size.isCalcSize() && !size.isSpecified()) {
        return keywordBorderBoxBlockSize().transform([&](auto basisBlockSize) {
            return BlockFormattingUtils::calcSizeBorderBoxSize(size.calcSize(), style, basisBlockSize, borderAndPaddingBlockSize, containingBlockContentBoxBlockSize.value_or(0_lu));
        });
    }
    if (size.isStretch() || size.isIntrinsic())
        return keywordBorderBoxBlockSize();
    if (!size.isSpecified())
        return { };
    if (isPercentageOfContainingBlock(size) && !containingBlockContentBoxBlockSize)
        return { };
    auto value = Style::evaluate<LayoutUnit>(size, containingBlockContentBoxBlockSize.value_or(0_lu), style.usedZoomForLength());
    if (style.boxSizing() == BoxSizing::BorderBox)
        return std::max(value, borderAndPaddingBlockSize);
    return value + borderAndPaddingBlockSize;
}

LayoutUnit BlockFormattingUtils::borderBoxBlockSize(const RenderBox& box, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto blockSize = borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, intrinsicContentBlockSize).value_or(intrinsicContentBlockSize.value_or(0_lu) + borderAndPaddingBlockSize);
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, intrinsicContentBlockSize))
        blockSize = std::min(blockSize, *maximumBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, intrinsicContentBlockSize))
        blockSize = std::max(blockSize, *minimumBlockSize);
    return blockSize;
}

std::optional<LayoutUnit> BlockFormattingUtils::aspectRatioBorderBoxBlockSize(const RenderBox& box, LayoutUnit borderBoxInlineSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode, std::optional<LayoutUnit> intrinsicContentBlockSize)
{
    CheckedRef style = box.style();
    // The intrinsic sizing keywords in the block axis are the automatic size (css-sizing-3 §3.2).
    auto& blockSize = style->logicalHeight(writingMode);
    if (!style->aspectRatio().hasRatio() || !(blockSize.isAuto() || blockSize.isIntrinsic()))
        return { };
    auto ratio = style->logicalAspectRatio();
    if (!ratio || std::isnan(ratio))
        return { };
    // With box-sizing: border-box the ratio is the border box's, unless it is auto && <ratio>, which is the content box's.
    auto borderBoxBlockSize = [&] {
        if (style->boxSizingForAspectRatio() == BoxSizing::BorderBox && style->aspectRatio().isRatio())
            return std::max(verticalBorderAndPadding, LayoutUnit(borderBoxInlineSize / ratio));
        return LayoutUnit((borderBoxInlineSize - horizontalBorderAndPadding) / ratio) + verticalBorderAndPadding;
    }();
    // The size the ratio gives is the basis of a calc-size().
    if (blockSize.isCalcSize())
        borderBoxBlockSize = calcSizeBorderBoxSize(blockSize.calcSize(), style.get(), borderBoxBlockSize, verticalBorderAndPadding, containingBlockContentBoxBlockSize.value_or(0_lu));
    // The intrinsic sizing keywords are the size the ratio gives (RenderBox::computeIntrinsicKeywordLogicalWidths), which they do not change.
    auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize);
    auto& minimumSize = style->logicalMinHeight(writingMode);
    auto minimumBlockSize = [&] -> std::optional<LayoutUnit> {
        if (!intrinsicContentBlockSize || !minimumSize.isAuto() || box.effectiveOverflowBlockDirection() != Overflow::Visible)
            return borderBoxBlockSizeFor(minimumSize, style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize);
        // The automatic minimum block size, which is also the basis of a calc-size().
        auto automaticMinimumBlockSize = std::min(*intrinsicContentBlockSize + verticalBorderAndPadding, maximumBlockSize.value_or(LayoutUnit::max()));
        if (!minimumSize.isCalcSize())
            return automaticMinimumBlockSize;
        return calcSizeBorderBoxSize(minimumSize.calcSize(), style.get(), automaticMinimumBlockSize, verticalBorderAndPadding, containingBlockContentBoxBlockSize.value_or(0_lu));
    }();
    if (maximumBlockSize)
        borderBoxBlockSize = std::min(borderBoxBlockSize, *maximumBlockSize);
    if (minimumBlockSize)
        borderBoxBlockSize = std::max(borderBoxBlockSize, *minimumBlockSize);
    return borderBoxBlockSize;
}

bool BlockFormattingUtils::hasFillAvailableBlockSize(const RenderBox& box, WritingMode writingMode)
{
    CheckedRef style = box.style();
    return style->logicalHeight(writingMode).isFillAvailable() || style->logicalMinHeight(writingMode).isFillAvailable() || style->logicalMaxHeight(writingMode).isFillAvailable();
}

LayoutUnit BlockFormattingUtils::fillAvailableBlockSize(const RenderBox& blockContainer, const BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, LayoutUnit containingBlockFillAvailableBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    CheckedRef style = blockContainer.style();
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    // An anonymous block box or one with an auto block size passes the percentages through, but not this.
    if (contentBoxBlockSize && !blockContainer.isAnonymous() && !style->logicalHeight(writingMode).isAuto())
        return *contentBoxBlockSize;
    auto blockSize = containingBlockFillAvailableBlockSize - geometry.marginBefore() - geometry.marginAfter() - borderAndPaddingBlockSize;
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        blockSize = std::min(blockSize, *maximumBlockSize - borderAndPaddingBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        blockSize = std::max(blockSize, *minimumBlockSize - borderAndPaddingBlockSize);
    return blockSize;
}

bool BlockFormattingUtils::hasPercentageBlockSize(const RenderBox& box, WritingMode writingMode)
{
    CheckedRef style = box.style();
    // Stretch behaves like 100% (css-sizing-4 §6.7).
    auto dependsOnContainingBlock = [](auto& size) {
        return size.isPercentOrCalculated() || size.isStretch() || (size.isCalcSize() && size.calcSize().hasPercentage());
    };
    return dependsOnContainingBlock(style->logicalHeight(writingMode)) || dependsOnContainingBlock(style->logicalMinHeight(writingMode)) || dependsOnContainingBlock(style->logicalMaxHeight(writingMode));
}

std::optional<LayoutUnit> BlockFormattingUtils::emptyEditableContentBlockSize(const RenderBox& blockContainer)
{
    CheckedPtr renderBlock = dynamicDowncast<RenderBlock>(blockContainer);
    if (!renderBlock || !renderBlock->hasLineIfEmpty())
        return { };
    CheckedRef firstLineStyle = blockContainer.firstLineStyle();
    return LayoutUnit::fromFloatCeil(firstLineStyle->usedLineHeight());
}

bool BlockFormattingUtils::blockSizeBehavesAsAuto(const Style::PreferredSize& blockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize)
{
    return blockSize.isAuto() || blockSize.isIntrinsic() || ((isPercentageOfContainingBlock(blockSize) || blockSize.isStretch()) && !containingBlockContentBoxBlockSize);
}

std::optional<LayoutUnit> BlockFormattingUtils::definiteContentBoxBlockSize(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    // Anonymous block boxes are ignored when resolving percentage values that would refer to them (CSS 2.2 §9.2.1.1).
    if (box.isAnonymous())
        return containingBlockContentBoxBlockSize;
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    CheckedRef style = box.style();
    // In quirks mode so are the ones with an auto block size, and those of the body and the root element take their margins, borders and
    // padding with them (quirks §3.5, RenderBox::skipContainingBlockForPercentHeightCalculation).
    if (box.document().inQuirksMode() && !box.isOutOfFlowPositioned() && style->logicalHeight(writingMode).isAuto()) {
        if (!box.isBody() && !box.isDocumentElementRenderer())
            return containingBlockContentBoxBlockSize;
        return containingBlockContentBoxBlockSize.transform([&](auto blockSize) {
            return std::max(0_lu, blockSize - geometry.marginBefore() - geometry.marginAfter() - borderAndPaddingBlockSize);
        });
    }
    // A preferred aspect ratio gives an auto block size from the definite inline size.
    if (auto aspectRatioBlockSize = aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, writingMode))
        return *aspectRatioBlockSize - borderAndPaddingBlockSize;
    if (!borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        return { };
    return borderBoxBlockSize(box, borderAndPaddingBlockSize, { }, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize, writingMode) - borderAndPaddingBlockSize;
}

bool BlockFormattingUtils::hasIntrinsicInlineSize(const RenderBox& box, WritingMode writingMode)
{
    CheckedRef style = box.style();
    return style->logicalWidth(writingMode).isIntrinsic() || style->logicalMinWidth(writingMode).isIntrinsic() || style->logicalMaxWidth(writingMode).isIntrinsic();
}

// The border box inline size the preferred aspect ratio of a box gives a border box block size (like RenderBox's inlineSizeFromAspectRatio).
static std::optional<LayoutUnit> aspectRatioBorderBoxInlineSizeFor(const Style::ComputedStyle& style, LayoutUnit borderBoxBlockSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding)
{
    if (!style.aspectRatio().hasRatio())
        return { };
    auto ratio = style.logicalAspectRatio();
    if (!ratio || std::isnan(ratio))
        return { };
    // With box-sizing: border-box the ratio is the border box's, unless it is auto && <ratio>, which is the content box's.
    if (style.boxSizingForAspectRatio() == BoxSizing::BorderBox && style.aspectRatio().isRatio())
        return std::max(horizontalBorderAndPadding, LayoutUnit(borderBoxBlockSize * ratio));
    return LayoutUnit((borderBoxBlockSize - verticalBorderAndPadding) * ratio) + horizontalBorderAndPadding;
}

std::optional<LayoutUnit> BlockFormattingUtils::aspectRatioBorderBoxInlineSize(const RenderBox& box, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    if (!style->aspectRatio().hasRatio())
        return { };
    auto blockSize = borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize);
    if (!blockSize)
        return { };
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        blockSize = std::min(*blockSize, *maximumBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        blockSize = std::max(*blockSize, *minimumBlockSize);
    return aspectRatioBorderBoxInlineSizeFor(style.get(), *blockSize, horizontalBorderAndPadding, verticalBorderAndPadding);
}

bool BlockFormattingUtils::hasAspectRatioAutomaticMinimumInlineSize(const RenderBox& box, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& inlineSize = style->logicalWidth(writingMode);
    return aspectRatioBorderBoxInlineSize && style->logicalMinWidth(writingMode).isAuto() && (inlineSize.isAuto() || inlineSize.isMinContent() || inlineSize.isMaxContent())
        && box.effectiveOverflowInlineDirection() == Overflow::Visible;
}

std::optional<std::pair<LayoutUnit, LayoutUnit>> BlockFormattingUtils::aspectRatioInlineSizeLimits(const RenderBox& box, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, std::optional<LayoutUnit> stretchFitBorderBoxBlockSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    if (!aspectRatioBorderBoxInlineSizeFor(style.get(), 0_lu, horizontalBorderAndPadding, verticalBorderAndPadding))
        return { };
    auto borderBoxInlineSizeFor = [&](LayoutUnit borderBoxBlockSize) {
        return *aspectRatioBorderBoxInlineSizeFor(style.get(), borderBoxBlockSize, horizontalBorderAndPadding, verticalBorderAndPadding);
    };
    auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize).value_or(0_lu);
    auto minimum = minimumBlockSize ? borderBoxInlineSizeFor(minimumBlockSize) : 0_lu;
    auto maximum = LayoutUnit::max();
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSize))
        maximum = borderBoxInlineSizeFor(std::max(*maximumBlockSize, minimumBlockSize));
    return std::pair { minimum, std::max(minimum, maximum) };
}

auto BlockFormattingUtils::inlineSizeAndMargins(const RenderBox& box, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<IntrinsicWidthConstraints> contentInlineSizes, std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize) -> InlineSizeAndMargins
{
    auto containingBlockWritingMode = containingBlockStyle.writingMode();
    CheckedRef style = box.style();
    auto zoom = style->usedZoomForLength();
    auto& marginStartLength = style->marginStart(containingBlockWritingMode);
    auto& marginEndLength = style->marginEnd(containingBlockWritingMode);
    auto usedMargin = [&](auto& margin) {
        return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, containingBlockInlineSize, zoom);
    };
    auto marginStart = usedMargin(marginStartLength);
    auto marginEnd = usedMargin(marginEndLength);
    // The stretch-fit inline size, auto margins as zero (css-sizing-3 §5.1).
    auto stretchFitInlineSize = std::max(borderAndPaddingInlineSize, containingBlockInlineSize - marginStart - marginEnd);
    auto keywordBorderBoxInlineSizeFor = [&](auto& size) {
        if (size.isStretch())
            return stretchFitInlineSize;
        ASSERT(contentInlineSizes);
        auto minContentSize = contentInlineSizes->minimum + borderAndPaddingInlineSize;
        auto maxContentSize = contentInlineSizes->maximum + borderAndPaddingInlineSize;
        if (size.isMinContent())
            return minContentSize;
        if (size.isMaxContent())
            return maxContentSize;
        // The fit-content size: the stretch-fit size, floored by the min-content size and capped by the max-content size.
        return std::min(std::max(minContentSize, stretchFitInlineSize), maxContentSize);
    };
    // A calc-size() with a keyword basis resolves its calculation with the size of the keyword (css-values-5 §11.1).
    auto calcSizeBorderBoxInlineSizeFor = [&](auto& size, LayoutUnit basisInlineSize) {
        return size.isCalcSize() ? calcSizeBorderBoxSize(size.calcSize(), style.get(), basisInlineSize, borderAndPaddingInlineSize, containingBlockInlineSize) : basisInlineSize;
    };
    auto borderBoxInlineSizeFor = [&](auto& size) {
        if (size.isIntrinsicOrStretch())
            return calcSizeBorderBoxInlineSizeFor(size, keywordBorderBoxInlineSizeFor(size));
        auto value = Style::evaluate<LayoutUnit>(size, containingBlockInlineSize, zoom);
        if (style->boxSizing() == BoxSizing::BorderBox)
            return std::max(value, borderAndPaddingInlineSize);
        return value + borderAndPaddingInlineSize;
    };

    auto borderBoxInlineSize = [&] {
        auto& inlineSize = style->logicalWidth(containingBlockWritingMode);
        if (inlineSize.isSpecified() || inlineSize.isIntrinsicOrStretch())
            return borderBoxInlineSizeFor(inlineSize);
        auto autoInlineSize = [&] {
            if (aspectRatioBorderBoxInlineSize)
                return *aspectRatioBorderBoxInlineSize;
            // An auto inline size fits the content of a box justify-self aligns.
            if (isAlignedByJustifySelf(box, containingBlockStyle)) {
                ASSERT(contentInlineSizes);
                return std::min(std::max(contentInlineSizes->minimum + borderAndPaddingInlineSize, stretchFitInlineSize), contentInlineSizes->maximum + borderAndPaddingInlineSize);
            }
            return stretchFitInlineSize;
        }();
        return calcSizeBorderBoxInlineSizeFor(inlineSize, autoInlineSize);
    }();
    auto& maximumWidth = style->logicalMaxWidth(containingBlockWritingMode);
    auto& minimumWidth = style->logicalMinWidth(containingBlockWritingMode);
    auto maximum = maximumWidth.isSpecified() || maximumWidth.isIntrinsicOrStretch() ? borderBoxInlineSizeFor(maximumWidth) : LayoutUnit::max();
    auto minimum = [&] {
        if (minimumWidth.isSpecified() || minimumWidth.isIntrinsicOrStretch())
            return borderBoxInlineSizeFor(minimumWidth);
        // The automatic minimum inline size is zero, unless a preferred aspect ratio gives the inline size, where the content sizes already
        // went through a calc-size() (BlockFormattingContext::computeInlineSizeAndMargins).
        if (hasAspectRatioAutomaticMinimumInlineSize(box, aspectRatioBorderBoxInlineSize, containingBlockWritingMode)) {
            ASSERT(contentInlineSizes);
            return std::min(contentInlineSizes->minimum + borderAndPaddingInlineSize, maximum);
        }
        return calcSizeBorderBoxInlineSizeFor(minimumWidth, borderAndPaddingInlineSize);
    }();
    // The limits a preferred aspect ratio transfers apply where the box has no min or max length or percentage of its own, within the one it
    // has (like RenderBox::constrainLogicalMinMaxSizesByAspectRatio).
    if (aspectRatioInlineSizeLimits) {
        auto [transferredMinimum, transferredMaximum] = *aspectRatioInlineSizeLimits;
        if (!maximumWidth.isSpecified() && transferredMaximum != LayoutUnit::max())
            maximum = std::min(maximum, minimumWidth.isSpecified() ? std::max(transferredMaximum, minimum) : transferredMaximum);
        if (!minimumWidth.isSpecified() && transferredMinimum > 0)
            minimum = std::max(minimum, maximumWidth.isSpecified() ? std::min(transferredMinimum, maximum) : transferredMinimum);
    }
    borderBoxInlineSize = std::max(std::min(borderBoxInlineSize, maximum), minimum);

    std::tie(marginStart, marginEnd) = inlineMargins(box, borderBoxInlineSize, containingBlockInlineSize, containingBlockStyle);
    return { borderBoxInlineSize, marginStart, marginEnd };
}

std::pair<LayoutUnit, LayoutUnit> BlockFormattingUtils::inlineMargins(const RenderBox& box, LayoutUnit borderBoxInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle)
{
    auto containingBlockWritingMode = containingBlockStyle.writingMode();
    CheckedRef style = box.style();
    auto& marginStartLength = style->marginStart(containingBlockWritingMode);
    auto& marginEndLength = style->marginEnd(containingBlockWritingMode);
    auto usedMargin = [&](auto& margin) {
        return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, containingBlockInlineSize, style->usedZoomForLength());
    };
    auto marginStart = usedMargin(marginStartLength);
    auto marginEnd = usedMargin(marginEndLength);
    // The legacy -webkit-center, -webkit-left and -webkit-right values of text-align align a box without auto margins in the containing block
    // the way auto margins would (RenderBox::computeInlineDirectionMargins). -webkit-center centers the margin box, without moving it past the
    // start of the containing block.
    // justify-self takes over from them, and from the over-constrained end margin.
    auto isAlignedByJustifySelf = BlockFormattingUtils::isAlignedByJustifySelf(box, containingBlockStyle);
    auto textAlign = isAlignedByJustifySelf ? Style::TextAlign::Start : containingBlockStyle.textAlign();
    auto isLeftToRight = containingBlockWritingMode.isBidiLTR();
    auto hasAutoMargin = marginStartLength.isAuto() || marginEndLength.isAuto();
    if (!hasAutoMargin && textAlign == Style::TextAlign::WebKitCenter) {
        auto centeredMarginBoxStart = std::max(0_lu, (containingBlockInlineSize - borderBoxInlineSize - marginStart - marginEnd) / 2);
        marginEnd = containingBlockInlineSize - borderBoxInlineSize - centeredMarginBoxStart - marginStart + marginEnd;
        marginStart += centeredMarginBoxStart;
    } else if (borderBoxInlineSize < containingBlockInlineSize) {
        auto pushesToEnd = !marginEndLength.isAuto() && (isLeftToRight ? textAlign == Style::TextAlign::WebKitRight : textAlign == Style::TextAlign::WebKitLeft);
        if (marginStartLength.isAuto() && marginEndLength.isAuto()) {
            marginStart = std::max(0_lu, (containingBlockInlineSize - borderBoxInlineSize) / 2);
            marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
        } else if (marginEndLength.isAuto())
            marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
        else if (marginStartLength.isAuto() || pushesToEnd)
            marginStart = containingBlockInlineSize - borderBoxInlineSize - marginEnd;
    }
    // An over-constrained box ignores its end margin, which takes what makes the equality true. Like RenderBox::computeLogicalWidth,
    // the end margin keeps its value in a scroll container, where it adds to the scrollable overflow, and for an orthogonal box.
    auto isScrollContainer = [&](Overflow overflow) {
        return overflow != Overflow::Visible && overflow != Overflow::Clip;
    };
    auto isOrthogonal = style->writingMode().isOrthogonal(containingBlockWritingMode);
    if (containingBlockInlineSize && !isAlignedByJustifySelf && !isOrthogonal && !isScrollContainer(containingBlockStyle.overflowX()) && !isScrollContainer(containingBlockStyle.overflowY()))
        marginEnd = containingBlockInlineSize - borderBoxInlineSize - marginStart;
    return { marginStart, marginEnd };
}

bool BlockFormattingUtils::isAlignedByJustifySelf(const RenderBox& box, const Style::ComputedStyle& containingBlockStyle)
{
    CheckedRef style = box.style();
    return !box.isAnonymous() && !style->justifySelf().resolve(&containingBlockStyle).isNormalStretchOrLegacy();
}

LayoutUnit BlockFormattingUtils::justifySelfOffset(const RenderBox& box, LayoutUnit borderBoxInlineSize, LayoutUnit marginStart, LayoutUnit marginEnd, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle)
{
    auto containingBlockWritingMode = containingBlockStyle.writingMode();
    CheckedRef style = box.style();
    if (!isAlignedByJustifySelf(box, containingBlockStyle) || style->marginStart(containingBlockWritingMode).isAuto() || style->marginEnd(containingBlockWritingMode).isAuto())
        return { };
    auto justifySelf = style->justifySelf().resolve(&containingBlockStyle);
    auto freeSpace = containingBlockInlineSize - borderBoxInlineSize - marginStart - marginEnd;
    if (justifySelf.overflow() == OverflowAlignment::Safe)
        freeSpace = std::max(0_lu, freeSpace);
    return StyleSelfAlignmentData::adjustmentFromStartEdge(freeSpace, justifySelf.position(), LogicalBoxAxis::Inline, containingBlockWritingMode, box.writingMode());
}

}
}
