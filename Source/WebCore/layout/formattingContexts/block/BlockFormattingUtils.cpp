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

#include "HTMLButtonElement.h"
#include "HTMLInputElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "LayoutBoxGeometry.h"
#include "LocalFrameViewLayoutContext.h"
#include "RenderAncestorIterator.h"
#include "RenderBlockFlow.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
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

// The legacy intrinsic and min-intrinsic keywords, which resolve like fit-content and min-content (RenderBox::computeLogicalWidthUsing).
template<typename SizeType>
static bool isLegacyIntrinsicKeyword(const SizeType& size)
{
    return size.isIntrinsicKeyword() || size.isMinIntrinsic();
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
static std::optional<LayoutUnit> borderBoxBlockSizeFor(const SizeType& size, const Style::ComputedStyle& style, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, std::optional<LayoutUnit> intrinsicContentBlockSize = { }, BlockFormattingUtils::ContainingBlockIsTableCell containingBlockIsTableCell = BlockFormattingUtils::ContainingBlockIsTableCell::No)
{
    auto keywordBorderBoxBlockSize = [&] -> std::optional<LayoutUnit> {
        // Without a definite stretch-fit block size, stretch behaves as the initial value (css-sizing-4 §6.1).
        if (size.isStretch()) {
            return stretchFitBorderBoxBlockSizes.forSize(size).transform([&](auto stretchFitSize) {
                return std::max(stretchFitSize, borderAndPaddingBlockSize);
            });
        }
        // The intrinsic sizing keywords, the legacy ones included, are the block size of the content, once it is laid out (css-sizing-3 §3.2,
        // RenderBox::computeSizingKeywordLogicalContentHeightUsingGeneric), and so is auto, but for the min block size, where it is zero.
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
    if (size.isStretch() || size.isIntrinsic() || isLegacyIntrinsicKeyword(size))
        return keywordBorderBoxBlockSize();
    if (!size.isSpecified())
        return { };
    if (isPercentageOfContainingBlock(size) && !containingBlockContentBoxBlockSize)
        return { };
    auto value = Style::evaluate<LayoutUnit>(size, containingBlockContentBoxBlockSize.value_or(0_lu), style.usedZoomForLength());
    // A stretch() fits the margin box to its argument, whatever the box-sizing (css-sizing-4 §3.1).
    if (size.isStretchFunction())
        return std::max(value - stretchFitBorderBoxBlockSizes.stretchFunctionMargins, borderAndPaddingBlockSize);
    if (style.boxSizing() == BoxSizing::BorderBox || (containingBlockIsTableCell == BlockFormattingUtils::ContainingBlockIsTableCell::Yes && size.isPercentOrCalculated()))
        return std::max(value, borderAndPaddingBlockSize);
    return value + borderAndPaddingBlockSize;
}

LayoutUnit BlockFormattingUtils::borderBoxBlockSize(const RenderBox& box, LayoutUnit borderAndPaddingBlockSize, std::optional<LayoutUnit> intrinsicContentBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode, ContainingBlockIsTableCell containingBlockIsTableCell)
{
    CheckedRef style = box.style();
    auto blockSize = borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, intrinsicContentBlockSize, containingBlockIsTableCell).value_or(intrinsicContentBlockSize.value_or(0_lu) + borderAndPaddingBlockSize);
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, intrinsicContentBlockSize, containingBlockIsTableCell))
        blockSize = std::min(blockSize, *maximumBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, intrinsicContentBlockSize, containingBlockIsTableCell))
        blockSize = std::max(blockSize, *minimumBlockSize);
    return blockSize;
}

std::optional<LayoutUnit> BlockFormattingUtils::aspectRatioBorderBoxBlockSize(const RenderBox& box, LayoutUnit borderBoxInlineSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode, std::optional<LayoutUnit> intrinsicContentBlockSize)
{
    CheckedRef style = box.style();
    // The intrinsic sizing keywords in the block axis are the automatic size (css-sizing-3 §3.2), and so is a percentage or stretch that does
    // not resolve (CSS 2.2 §10.5, css-sizing-4 §6.1, RenderBox::shouldComputeLogicalHeightFromAspectRatio).
    auto& blockSize = style->logicalHeight(writingMode);
    auto isAutomatic = [&] {
        if (blockSize.isAuto() || blockSize.isIntrinsic())
            return true;
        if (box.isOutOfFlowPositioned() || !(blockSize.isPercentOrCalculated() || blockSize.isStretch()))
            return false;
        return !borderBoxBlockSizeFor(blockSize, style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes);
    };
    if (!style->aspectRatio().hasRatio() || !isAutomatic())
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
    auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes);
    auto& minimumSize = style->logicalMinHeight(writingMode);
    auto minimumBlockSize = [&] -> std::optional<LayoutUnit> {
        if (!intrinsicContentBlockSize || !minimumSize.isAuto() || box.effectiveOverflowBlockDirection() != Overflow::Visible)
            return borderBoxBlockSizeFor(minimumSize, style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes);
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

LayoutUnit BlockFormattingUtils::availableBlockSize(const RenderBox& blockContainer, const BoxGeometry& geometry, std::optional<LayoutUnit> contentBoxBlockSize, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, LayoutUnit containingBlockAvailableBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode, AvailableLogicalHeightType type)
{
    CheckedRef style = blockContainer.style();
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    // An anonymous block box or one with an auto block size passes the percentages through, but not this.
    if (contentBoxBlockSize && !blockContainer.isAnonymous() && !style->logicalHeight(writingMode).isAuto())
        return *contentBoxBlockSize;
    // A preferred aspect ratio gives an auto block size from the inline size (RenderBox::availableLogicalHeightUsing).
    if (auto aspectRatioBlockSize = aspectRatioBorderBoxBlockSize(blockContainer, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode))
        return type == AvailableLogicalHeightType::ExcludeMarginBorderPadding ? *aspectRatioBlockSize - borderAndPaddingBlockSize : *aspectRatioBlockSize;
    // Without the scrollbar in the block axis (RenderBox::availableLogicalHeightUsing).
    auto scrollbarBlockSize = geometry.verticalSpaceForScrollbar();
    auto blockSize = containingBlockAvailableBlockSize;
    if (type == AvailableLogicalHeightType::ExcludeMarginBorderPadding)
        blockSize -= geometry.marginBefore() + geometry.marginAfter() + borderAndPaddingBlockSize + scrollbarBlockSize;
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes))
        blockSize = std::min(blockSize, *maximumBlockSize - borderAndPaddingBlockSize - scrollbarBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes))
        blockSize = std::max(blockSize, *minimumBlockSize - borderAndPaddingBlockSize - scrollbarBlockSize);
    return blockSize;
}

// Stretch resolves against a block size the containing block has of its own, not the one an auto block size passes on to percentages in quirks
// mode, except for the body and the root element, which stretch to the viewport, and a flex or grid item, which its container sizes
// (RenderBox::isBlockSizeResolvableForStretch).
std::optional<LayoutUnit> BlockFormattingUtils::stretchFitBasis(const RenderBox& containingBlock, std::optional<LayoutUnit> contentBoxBlockSize)
{
    if (containingBlock.document().inQuirksMode() && containingBlock.style().logicalHeight().isAuto() && !containingBlock.isAnonymous()
        && !containingBlock.isOutOfFlowPositioned() && !containingBlock.isBody() && !containingBlock.isDocumentElementRenderer()
        && !containingBlock.isFlexItem() && !containingBlock.isGridItem())
        return { };
    return contentBoxBlockSize;
}

// Whether the block sizes of a box that depend on the block size of its containing block are all stretch ones.
bool BlockFormattingUtils::hasOnlyStretchBlockSizes(const RenderBox& box)
{
    CheckedRef style = box.style();
    auto writingMode = box.writingMode();
    auto isStretch = [](auto& size) {
        return size.isStretch() && !size.isFillAvailable();
    };
    auto isPercentage = [](auto& size) {
        return size.isPercentOrCalculated() || (size.isCalcSize() && size.calcSize().hasPercentage());
    };
    auto& height = style->logicalHeight(writingMode);
    auto& minHeight = style->logicalMinHeight(writingMode);
    auto& maxHeight = style->logicalMaxHeight(writingMode);
    return (isStretch(height) || isStretch(minHeight) || isStretch(maxHeight)) && !isPercentage(height) && !isPercentage(minHeight) && !isPercentage(maxHeight);
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

std::optional<LayoutUnit> BlockFormattingUtils::definiteContentBoxBlockSize(const RenderBox& box, const BoxGeometry& geometry, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode, ContainingBlockIsTableCell containingBlockIsTableCell)
{
    auto passedThroughContainingBlockContentBoxBlockSize = containingBlockIsTableCell == ContainingBlockIsTableCell::Yes ? std::nullopt : containingBlockContentBoxBlockSize;
    // Anonymous block boxes are ignored when resolving percentage values that would refer to them (CSS 2.2 §9.2.1.1).
    if (box.isAnonymous())
        return passedThroughContainingBlockContentBoxBlockSize;
    auto borderAndPaddingBlockSize = geometry.verticalBorderAndPadding();
    CheckedRef style = box.style();
    // In quirks mode so are the ones with an auto block size, and those of the body and the root element take their margins, borders and
    // padding with them (quirks §3.5, RenderBox::skipContainingBlockForPercentHeightCalculation).
    if (box.document().inQuirksMode() && !box.isOutOfFlowPositioned() && style->logicalHeight(writingMode).isAuto()) {
        if (!box.isBody() && !box.isDocumentElementRenderer())
            return passedThroughContainingBlockContentBoxBlockSize;
        return containingBlockContentBoxBlockSize.transform([&](auto blockSize) {
            return std::max(0_lu, blockSize - geometry.marginBefore() - geometry.marginAfter() - borderAndPaddingBlockSize);
        });
    }
    // A preferred aspect ratio gives an auto block size from the definite inline size. The scrollbar in the block axis is not part of it
    // (RenderBox::computePercentageLogicalHeight).
    auto scrollbarBlockSize = geometry.verticalSpaceForScrollbar();
    if (auto aspectRatioBlockSize = aspectRatioBorderBoxBlockSize(box, geometry.borderBoxWidth(), geometry.horizontalBorderAndPadding(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode))
        return std::max(0_lu, *aspectRatioBlockSize - borderAndPaddingBlockSize - scrollbarBlockSize);
    if (!borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), borderAndPaddingBlockSize, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, { }, containingBlockIsTableCell))
        return { };
    return std::max(0_lu, borderBoxBlockSize(box, borderAndPaddingBlockSize, { }, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes, writingMode, containingBlockIsTableCell) - borderAndPaddingBlockSize - scrollbarBlockSize);
}

bool BlockFormattingUtils::hasIntrinsicInlineSize(const RenderBox& box, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& inlineSize = style->logicalWidth(writingMode);
    auto& minimumInlineSize = style->logicalMinWidth(writingMode);
    auto& maximumInlineSize = style->logicalMaxWidth(writingMode);
    return inlineSize.isIntrinsic() || isLegacyIntrinsicKeyword(inlineSize) || minimumInlineSize.isIntrinsic() || minimumInlineSize.isMinIntrinsic() || maximumInlineSize.isIntrinsic() || maximumInlineSize.isMinIntrinsic();
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

std::optional<LayoutUnit> BlockFormattingUtils::aspectRatioBorderBoxInlineSize(const RenderBox& box, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode)
{
    CheckedRef style = box.style();
    if (!style->aspectRatio().hasRatio())
        return { };
    auto blockSize = borderBoxBlockSizeFor(style->logicalHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes);
    if (!blockSize)
        return { };
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes))
        blockSize = std::min(*blockSize, *maximumBlockSize);
    if (auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes))
        blockSize = std::max(*blockSize, *minimumBlockSize);
    return aspectRatioBorderBoxInlineSizeFor(style.get(), *blockSize, horizontalBorderAndPadding, verticalBorderAndPadding);
}

std::optional<LayoutUnit> BlockFormattingUtils::aspectRatioBorderBoxInlineSize(const RenderBox& box, LayoutUnit borderBoxBlockSize, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding)
{
    return aspectRatioBorderBoxInlineSizeFor(box.style(), borderBoxBlockSize, horizontalBorderAndPadding, verticalBorderAndPadding);
}

// The min-content and max-content sizes of a box whose preferred aspect ratio gives its inline size: the size the ratio gives, floored by the
// min-content size of its content when its min inline size is auto, the automatic minimum, through the calculation of a calc-size() there,
// where percentages are cyclic (RenderBox::computeIntrinsicKeywordLogicalWidths, applyAutomaticContentBasedMinimumSize).
IntrinsicWidthConstraints BlockFormattingUtils::aspectRatioContentInlineSizes(const RenderBox& box, LayoutUnit aspectRatioContentBoxInlineSize, IntrinsicWidthConstraints contentSizes, LayoutUnit borderAndPaddingInlineSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& minimumSize = style->logicalMinWidth(writingMode);
    if (!minimumSize.isAuto() || !box.firstChild())
        return { aspectRatioContentBoxInlineSize, aspectRatioContentBoxInlineSize };
    auto contentMinimum = contentSizes.minimum;
    if (minimumSize.isCalcSize())
        contentMinimum = calcSizeBorderBoxSize(minimumSize.calcSize(), style.get(), contentMinimum + borderAndPaddingInlineSize, borderAndPaddingInlineSize, 0_lu) - borderAndPaddingInlineSize;
    auto minimum = std::max(aspectRatioContentBoxInlineSize, contentMinimum);
    return { minimum, minimum };
}

bool BlockFormattingUtils::hasAspectRatioAutomaticMinimumInlineSize(const RenderBox& box, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize, WritingMode writingMode)
{
    CheckedRef style = box.style();
    auto& inlineSize = style->logicalWidth(writingMode);
    return aspectRatioBorderBoxInlineSize && style->logicalMinWidth(writingMode).isAuto() && (inlineSize.isAuto() || inlineSize.isMinContent() || inlineSize.isMaxContent())
        && box.effectiveOverflowInlineDirection() == Overflow::Visible;
}

std::optional<std::pair<LayoutUnit, LayoutUnit>> BlockFormattingUtils::aspectRatioInlineSizeLimits(const RenderBox& box, LayoutUnit horizontalBorderAndPadding, LayoutUnit verticalBorderAndPadding, std::optional<LayoutUnit> containingBlockContentBoxBlockSize, const StretchFitBorderBoxBlockSizes& stretchFitBorderBoxBlockSizes, WritingMode writingMode)
{
    CheckedRef style = box.style();
    if (!aspectRatioBorderBoxInlineSizeFor(style.get(), 0_lu, horizontalBorderAndPadding, verticalBorderAndPadding))
        return { };
    auto borderBoxInlineSizeFor = [&](LayoutUnit borderBoxBlockSize) {
        return *aspectRatioBorderBoxInlineSizeFor(style.get(), borderBoxBlockSize, horizontalBorderAndPadding, verticalBorderAndPadding);
    };
    auto minimumBlockSize = borderBoxBlockSizeFor(style->logicalMinHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes).value_or(0_lu);
    auto minimum = minimumBlockSize ? borderBoxInlineSizeFor(minimumBlockSize) : 0_lu;
    auto maximum = LayoutUnit::max();
    if (auto maximumBlockSize = borderBoxBlockSizeFor(style->logicalMaxHeight(writingMode), style.get(), verticalBorderAndPadding, containingBlockContentBoxBlockSize, stretchFitBorderBoxBlockSizes))
        maximum = borderBoxInlineSizeFor(std::max(*maximumBlockSize, minimumBlockSize));
    return std::pair { minimum, std::max(minimum, maximum) };
}

auto BlockFormattingUtils::inlineSizeAndMargins(const RenderBox& box, LayoutUnit borderAndPaddingInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<IntrinsicWidthConstraints> contentInlineSizes, std::optional<std::pair<LayoutUnit, LayoutUnit>> aspectRatioInlineSizeLimits, std::optional<LayoutUnit> aspectRatioBorderBoxInlineSize, std::optional<LayoutUnit> availableInlineSize) -> InlineSizeAndMargins
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
    // The stretch-fit inline size, auto margins as zero (css-sizing-3 §5.1), and what auto margins share: for a box that avoids floats, the
    // space they leave (§4.2), while percentages still refer to the containing block.
    auto spaceInlineSize = availableInlineSize.value_or(containingBlockInlineSize);
    auto stretchFitInlineSize = std::max(borderAndPaddingInlineSize, spaceInlineSize - marginStart - marginEnd);
    auto containingBlockStretchFitInlineSize = std::max(borderAndPaddingInlineSize, containingBlockInlineSize - marginStart - marginEnd);
    auto keywordBorderBoxInlineSizeFor = [&](auto& size) {
        if (size.isStretch())
            return stretchFitInlineSize;
        // The legacy intrinsic keyword as the min or max inline size is the stretch-fit inline size, in the space beside floats only with an
        // auto inline size (RenderBox::computeLogicalWidthUsing, RenderBox::shrinkToAvoidFloats).
        if constexpr (!std::same_as<std::remove_cvref_t<decltype(size)>, Style::PreferredSize>) {
            if (size.isIntrinsicKeyword())
                return style->logicalWidth(containingBlockWritingMode).isAuto() ? stretchFitInlineSize : containingBlockStretchFitInlineSize;
        }
        ASSERT(contentInlineSizes);
        auto minContentSize = contentInlineSizes->minimum + borderAndPaddingInlineSize;
        auto maxContentSize = contentInlineSizes->maximum + borderAndPaddingInlineSize;
        if (size.isMinContent() || size.isMinIntrinsic())
            return minContentSize;
        if (size.isMaxContent())
            return maxContentSize;
        // The fit-content size, which the legacy intrinsic keyword is too: the stretch-fit size, floored by the min-content size and capped by
        // the max-content size. The keyword fits the content into the containing block, not into the space beside floats
        // (RenderBox::computeSizingKeywordLogicalWidthUsing, RenderBox::sizesLogicalWidthToFitContent).
        return std::min(std::max(minContentSize, containingBlockStretchFitInlineSize), maxContentSize);
    };
    auto isKeyword = [](auto& size) {
        return size.isIntrinsicOrStretch() || isLegacyIntrinsicKeyword(size);
    };
    // A calc-size() with a keyword basis resolves its calculation with the size of the keyword (css-values-5 §11.1).
    auto calcSizeBorderBoxInlineSizeFor = [&](auto& size, LayoutUnit basisInlineSize) {
        return size.isCalcSize() ? calcSizeBorderBoxSize(size.calcSize(), style.get(), basisInlineSize, borderAndPaddingInlineSize, containingBlockInlineSize) : basisInlineSize;
    };
    // A stretch() fits the margin box to its argument, auto margins as zero, whatever the box-sizing (css-sizing-4 §3.1).
    auto stretchFunctionBorderBoxInlineSize = [&](LayoutUnit value) {
        return std::max(value - marginStart - marginEnd, borderAndPaddingInlineSize);
    };
    auto borderBoxInlineSizeFor = [&](auto& size) {
        // A fit-content() is the fit-content size with stretch() into its argument for the available space (css-sizing-4 §3.1).
        if (size.isFitContentFunction()) {
            ASSERT(contentInlineSizes);
            auto stretchInlineSize = stretchFunctionBorderBoxInlineSize(Style::evaluate<LayoutUnit>(size.fitContentArgument(), containingBlockInlineSize, zoom));
            return std::max(contentInlineSizes->minimum + borderAndPaddingInlineSize, std::min(contentInlineSizes->maximum + borderAndPaddingInlineSize, stretchInlineSize));
        }
        if (isKeyword(size))
            return calcSizeBorderBoxInlineSizeFor(size, keywordBorderBoxInlineSizeFor(size));
        auto value = Style::evaluate<LayoutUnit>(size, containingBlockInlineSize, zoom);
        if (size.isStretchFunction())
            return stretchFunctionBorderBoxInlineSize(value);
        if (style->boxSizing() == BoxSizing::BorderBox)
            return std::max(value, borderAndPaddingInlineSize);
        return value + borderAndPaddingInlineSize;
    };

    auto borderBoxInlineSize = [&] {
        auto& inlineSize = style->logicalWidth(containingBlockWritingMode);
        if (inlineSize.isSpecified() || isKeyword(inlineSize))
            return borderBoxInlineSizeFor(inlineSize);
        auto autoInlineSize = [&] {
            if (aspectRatioBorderBoxInlineSize)
                return *aspectRatioBorderBoxInlineSize;
            if (autoInlineSizeFitsContent(box, containingBlockStyle)) {
                ASSERT(contentInlineSizes);
                return std::min(std::max(contentInlineSizes->minimum + borderAndPaddingInlineSize, stretchFitInlineSize), contentInlineSizes->maximum + borderAndPaddingInlineSize);
            }
            return stretchFitInlineSize;
        }();
        return calcSizeBorderBoxInlineSizeFor(inlineSize, autoInlineSize);
    }();
    auto& maximumWidth = style->logicalMaxWidth(containingBlockWritingMode);
    auto& minimumWidth = style->logicalMinWidth(containingBlockWritingMode);
    auto maximum = maximumWidth.isSpecified() || isKeyword(maximumWidth) ? borderBoxInlineSizeFor(maximumWidth) : LayoutUnit::max();
    auto minimum = [&] {
        if (minimumWidth.isSpecified() || isKeyword(minimumWidth))
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

    std::tie(marginStart, marginEnd) = inlineMargins(box, borderBoxInlineSize, containingBlockInlineSize, containingBlockStyle, availableInlineSize);
    return { borderBoxInlineSize, marginStart, marginEnd };
}

std::pair<LayoutUnit, LayoutUnit> BlockFormattingUtils::inlineMargins(const RenderBox& box, LayoutUnit borderBoxInlineSize, LayoutUnit containingBlockInlineSize, const Style::ComputedStyle& containingBlockStyle, std::optional<LayoutUnit> availableInlineSize)
{
    auto containingBlockWritingMode = containingBlockStyle.writingMode();
    CheckedRef style = box.style();
    auto& marginStartLength = style->marginStart(containingBlockWritingMode);
    auto& marginEndLength = style->marginEnd(containingBlockWritingMode);
    auto usedMargin = [&](auto& margin, LayoutUnit percentageBasis) {
        return margin.isAuto() ? 0_lu : Style::evaluateMinimum<LayoutUnit>(margin, percentageBasis, style->usedZoomForLength());
    };
    auto marginStart = usedMargin(marginStartLength, containingBlockInlineSize);
    auto marginEnd = usedMargin(marginEndLength, containingBlockInlineSize);
    // For a box that avoids floats, the margins that align it share the space they leave, which the percentages among them refer to, while
    // other percentages refer to the containing block (RenderBox::computeInlineDirectionMargins with containingBlockAvailableLineWidth).
    auto spaceInlineSize = availableInlineSize.value_or(containingBlockInlineSize);
    // The legacy -webkit-center, -webkit-left and -webkit-right values of text-align align a box without auto margins in the containing block
    // the way auto margins would (RenderBox::computeInlineDirectionMargins). -webkit-center centers the margin box, without moving it past the
    // start of the containing block.
    // justify-self takes over from them, and from the over-constrained end margin.
    auto isAlignedByJustifySelf = BlockFormattingUtils::isAlignedByJustifySelf(box, containingBlockStyle);
    auto textAlign = isAlignedByJustifySelf ? Style::TextAlign::Start : containingBlockStyle.textAlign();
    auto isLeftToRight = containingBlockWritingMode.isBidiLTR();
    auto hasAutoMargin = marginStartLength.isAuto() || marginEndLength.isAuto();
    if (!hasAutoMargin && textAlign == Style::TextAlign::WebKitCenter) {
        marginStart = usedMargin(marginStartLength, spaceInlineSize);
        marginEnd = usedMargin(marginEndLength, spaceInlineSize);
        auto centeredMarginBoxStart = std::max(0_lu, (spaceInlineSize - borderBoxInlineSize - marginStart - marginEnd) / 2);
        marginEnd = spaceInlineSize - borderBoxInlineSize - centeredMarginBoxStart - marginStart + marginEnd;
        marginStart += centeredMarginBoxStart;
    } else if (borderBoxInlineSize < spaceInlineSize) {
        auto pushesToEnd = !marginEndLength.isAuto() && (isLeftToRight ? textAlign == Style::TextAlign::WebKitRight : textAlign == Style::TextAlign::WebKitLeft);
        if (marginStartLength.isAuto() && marginEndLength.isAuto()) {
            marginStart = std::max(0_lu, (spaceInlineSize - borderBoxInlineSize) / 2);
            marginEnd = spaceInlineSize - borderBoxInlineSize - marginStart;
        } else if (marginEndLength.isAuto()) {
            marginStart = usedMargin(marginStartLength, spaceInlineSize);
            marginEnd = spaceInlineSize - borderBoxInlineSize - marginStart;
        } else if (marginStartLength.isAuto() || pushesToEnd) {
            marginEnd = usedMargin(marginEndLength, spaceInlineSize);
            marginStart = spaceInlineSize - borderBoxInlineSize - marginEnd;
        }
    }
    if (spaceInlineSize && ignoresEndMarginWhenOverConstrained(box, containingBlockStyle))
        marginEnd = spaceInlineSize - borderBoxInlineSize - marginStart;
    return { marginStart, marginEnd };
}

// An over-constrained box ignores its end margin, which takes what makes the equality true. Like RenderBox::computeLogicalWidth,
// the end margin keeps its value in a scroll container, where it adds to the scrollable overflow, and for an orthogonal box.
bool BlockFormattingUtils::ignoresEndMarginWhenOverConstrained(const RenderBox& box, const Style::ComputedStyle& containingBlockStyle)
{
    auto isScrollContainer = [&](Overflow overflow) {
        return overflow != Overflow::Visible && overflow != Overflow::Clip;
    };
    return !isAlignedByJustifySelf(box, containingBlockStyle) && !box.style().writingMode().isOrthogonal(containingBlockStyle.writingMode())
        && !isScrollContainer(containingBlockStyle.overflowX()) && !isScrollContainer(containingBlockStyle.overflowY());
}

bool BlockFormattingUtils::isAlignedByJustifySelf(const RenderBox& box, const Style::ComputedStyle& containingBlockStyle)
{
    CheckedRef style = box.style();
    return !box.isAnonymous() && !style->justifySelf().resolve(&containingBlockStyle).isNormalStretchOrLegacy();
}

// FIXME: The direction of a marquee is not mapped to its writing mode (RenderBox::sizesLogicalWidthToFitContent, RenderMarquee::direction).
static bool scrollsHorizontally(const RenderElement& marquee)
{
    switch (marquee.style().marqueeDirection()) {
    case MarqueeDirection::Auto:
    case MarqueeDirection::Forward:
    case MarqueeDirection::Backward:
    case MarqueeDirection::Left:
    case MarqueeDirection::Right:
        return true;
    case MarqueeDirection::Up:
    case MarqueeDirection::Down:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool BlockFormattingUtils::autoInlineSizeFitsContent(const RenderBox& box, const Style::ComputedStyle& containingBlockStyle)
{
    if (box.isFloating() || isAlignedByJustifySelf(box, containingBlockStyle))
        return true;
    if (isAnyOf<HTMLInputElement, HTMLSelectElement, HTMLButtonElement, HTMLTextAreaElement>(box.element()))
        return true;
    return box.parent()->isHTMLMarquee() && scrollsHorizontally(*box.parent());
}

bool BlockFormattingUtils::isScrollingHorizontalMarquee(const RenderBox& box)
{
    // One with behavior none does not scroll (RenderLayerScrollableArea::createOrDestroyMarquee).
    return box.isHTMLMarquee() && box.style().marqueeBehavior() != MarqueeBehavior::None && scrollsHorizontally(box);
}

bool BlockFormattingUtils::isFormControlBlockContainer(const RenderBox& box)
{
    return box.isRenderTextControl() || box.isRenderListBox() || box.isRenderProgress() || box.isRenderMeter() || box.isRenderFileUploadControl()
        || (box.type() == RenderObject::Type::BlockFlow && is<HTMLInputElement>(box.element()));
}

bool BlockFormattingUtils::establishesIndependentFormattingContext(const RenderBox& box)
{
    CheckedPtr blockContainer = dynamicDowncast<RenderBlockFlow>(box);
    return !blockContainer || blockContainer->createsNewFormattingContext() || isFormControlBlockContainer(box) || box.isRenderMultiColumnSet();
}

LayoutUnit BlockFormattingUtils::alignContentShift(const RenderBox& blockContainer, LayoutUnit freeSpace)
{
    if (blockContainer.isRenderTableCell() || is<HTMLInputElement>(blockContainer.element()))
        return { };
    auto alignment = blockContainer.style().alignContent().resolve();
    if (alignment.isNormal() || alignment.isStartward())
        return { };
    if (freeSpace <= 0) {
        auto isBlockAxisScrollContainer = blockContainer.isHorizontalWritingMode() ? blockContainer.isScrollContainerY() : blockContainer.isScrollContainerX();
        if ((alignment.overflow() == OverflowAlignment::Default && !isBlockAxisScrollContainer) || alignment.overflow() == OverflowAlignment::Safe || alignment.position() == ContentPosition::Normal)
            return { };
    }
    return alignment.isCentered() ? freeSpace / 2 : freeSpace;
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

// https://drafts.csswg.org/css-page-3/#using-named-pages
static AtomString usedPageValue(const RenderElement& renderer)
{
    for (CheckedRef ancestor : lineageOfType<RenderElement>(renderer)) {
        if (auto name = ancestor->style().pageName().tryValue())
            return name->value;
    }
    return emptyAtom();
}

enum class PageValueEdge : bool { Start, End };

static CheckedPtr<RenderBox> firstOrLastInFlowChild(const RenderBox& box, PageValueEdge edge)
{
    CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(box);
    if (!blockFlow || blockFlow->childrenInline())
        return nullptr;
    // A multicol container's content is in its flow. Column sets and spanners are siblings of the flow.
    if (CheckedPtr<RenderBox> multiColumnFlow = blockFlow->multiColumnFlow())
        return multiColumnFlow;
    CheckedPtr child = edge == PageValueEdge::Start ? blockFlow->firstInFlowChildBox() : blockFlow->lastInFlowChildBox();
    if (CheckedPtr placeholder = dynamicDowncast<RenderMultiColumnSpannerPlaceholder>(child))
        return placeholder->spanner();
    return child;
}

static AtomString startOrEndPageName(const RenderBox& box, PageValueEdge edge)
{
    AtomString name;
    for (CheckedPtr<const RenderBox> descendant = &box; descendant; descendant = firstOrLastInFlowChild(*descendant, edge)) {
        if (auto value = descendant->style().pageName().tryValue())
            name = value->value;
    }
    return name;
}

bool BlockFormattingUtils::pageValuesForceBreakBefore(const RenderBlockFlow& parent, const RenderBox& child)
{
    // A multicol container's children are its flow, column sets and spanners, not its content. Blocks inside inlines aren't supported.
    if (!parent.view().frameView().layoutContext().hasRenderersWithNamedPage() || parent.multiColumnFlow() || parent.childrenInline())
        return false;
    CheckedPtr previous = child.previousInFlowSiblingBox();
    if (!previous)
        return false;
    auto endName = startOrEndPageName(*previous, PageValueEdge::End);
    auto startName = startOrEndPageName(child, PageValueEdge::Start);
    if (endName == startName)
        return false;
    auto parentUsedValue = usedPageValue(parent);
    return (endName.isNull() ? parentUsedValue : endName) != (startName.isNull() ? parentUsedValue : startName);
}

}
}
