/*
 * Copyright (C) 2018-2024 Apple Inc. All rights reserved.
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
#include "InlineFormattingContext.h"

#include "AvailableLineWidthOverride.h"
#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineContentCache.h"
#include "InlineContentConstrainer.h"
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineInvalidation.h"
#include "InlineItemsBuilder.h"
#include "InlineLayoutState.h"
#include "InlineLineBox.h"
#include "InlineLineBoxBuilder.h"
#include "InlineLineTypes.h"
#include "InlineTextItem.h"
#include "IntrinsicWidthHandler.h"
#include "LayoutBox.h"
#include "LayoutContext.h"
#include "LayoutDescendantIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutIntegrationUtils.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RangeBasedLineBuilder.h"
#include "RenderStyleInlines.h"
#include "TextOnlySimpleLineBuilder.h"
#include "TextUtil.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(InlineContentCache);
WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(InlineFormattingContext);

static size_t estimatedDisplayBoxSize(size_t inlineItemSize)
{
    if (inlineItemSize == 1) {
        // Common case of blocks with only one word where we produce 2 boxes (root inline and text box)
        return 2;
    }
    static constexpr size_t maximumEstimatedDisplayBoxSize = 1000; // Let's try not to overwhelm vector's reserveInitialCapacity.
    // This value represents a simple average derived from typical web page content.
    return std::min<size_t>(maximumEstimatedDisplayBoxSize, inlineItemSize * 0.6);
}

static std::optional<InlineItemRange> partialRangeForDamage(const InlineItemList& inlineItemList, const InlineDamage& lineDamage)
{
    auto layoutStartPosition = lineDamage.layoutStartPosition()->inlineItemPosition;
    if (layoutStartPosition.index >= inlineItemList.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }
    auto* damagedInlineTextItem = dynamicDowncast<InlineTextItem>(inlineItemList[layoutStartPosition.index]);
    if (layoutStartPosition.offset && (!damagedInlineTextItem || layoutStartPosition.offset >= damagedInlineTextItem->length())) {
        ASSERT_NOT_REACHED();
        return { };
    }
    return InlineItemRange { layoutStartPosition, { inlineItemList.size(), 0 } };
}

static bool isEmptyInlineContent(const InlineItemList& inlineItemList)
{
    // Very common, pseudo before/after empty content.
    if (inlineItemList.size() != 1)
        return false;

    auto* inlineTextItem = dynamicDowncast<InlineTextItem>(inlineItemList[0]);
    return inlineTextItem && !inlineTextItem->length();
}

InlineFormattingContext::InlineFormattingContext(const ElementBox& rootBlockContainer, LayoutState& globalLayoutState, BlockLayoutState& parentBlockLayoutState)
    : m_rootBlockContainer(rootBlockContainer)
    , m_globalLayoutState(globalLayoutState)
    , m_floatingContext(rootBlockContainer, globalLayoutState, parentBlockLayoutState.placedFloats())
    , m_inlineFormattingUtils(*this)
    , m_inlineQuirks(*this)
    , m_integrationUtils(globalLayoutState)
    , m_inlineContentCache(globalLayoutState.inlineContentCache(rootBlockContainer))
    , m_inlineLayoutState(parentBlockLayoutState)
{
    initializeInlineLayoutState(globalLayoutState);
}

InlineLayoutResult InlineFormattingContext::layout(const ConstraintsForInlineContent& constraints, InlineDamage* lineDamage)
{
    rebuildInlineItemListIfNeeded(lineDamage);

    if (formattingUtils().shouldDiscardRemainingContentInBlockDirection()) {
        // This inline content may be completely collapsed (i.e. after clamped block container)
        resetBoxGeometriesForDiscardedContent({ { }, { inlineContentCache().inlineItems().content().size(), { } } }, { });
        return { };
    }

    if (!root().hasInFlowChild() && !root().hasOutOfFlowChild()) {
        // Float only content does not support partial layout.
        ASSERT(!InlineInvalidation::mayOnlyNeedPartialLayout(lineDamage));
        layoutFloatContentOnly(constraints);
        return { { }, InlineLayoutResult::Range::Full };
    }

    auto& inlineItems = inlineContentCache().inlineItems();
    auto& inlineItemList = inlineItems.content();
    auto needsLayoutRange = [&]() -> InlineItemRange {
        if (!InlineInvalidation::mayOnlyNeedPartialLayout(lineDamage))
            return { { }, { inlineItemList.size(), { } } };
        if (auto partialRange = partialRangeForDamage(inlineItemList, *lineDamage))
            return *partialRange;
        // We should be able to produce partial range for partial layout.
        ASSERT_NOT_REACHED();
        // Let's turn this unexpected state to full layout.
        lineDamage = nullptr;
        return { { }, { inlineItemList.size(), { } } };
    }();

    if (needsLayoutRange.isEmpty()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    auto previousLine = [&]() -> std::optional<PreviousLine> {
        if (!needsLayoutRange.start)
            return { };
        if (!lineDamage || !lineDamage->layoutStartPosition()) {
            ASSERT_NOT_REACHED();
            return { };
        }
        auto lastLineIndex = lineDamage->layoutStartPosition()->lineIndex - 1;
        // FIXME: We should be able to extract the last line information and provide it to layout as "previous line" (ends in line break and inline direction).
        return PreviousLine { lastLineIndex, { }, { }, { }, { } };
    }();

    auto& inlineLayoutState = layoutState();
    inlineLayoutState.setLineCount(previousLine ? previousLine->lineIndex + 1 : 0);
    // FIXME: This needs partial support when line-clamped content has nested blocks.
    inlineLayoutState.setLineCountWithInlineContentIncludingNestedBlocks(inlineLayoutState.lineCount());

    auto textWrapStyle = root().style().textWrapStyle();
    if (root().style().textWrapMode() == TextWrapMode::Wrap && (textWrapStyle == TextWrapStyle::Balance || textWrapStyle == TextWrapStyle::Pretty)) {
        auto constrainer = InlineContentConstrainer { *this, inlineItemList, constraints.horizontal() };
        auto constrainedLineWidths = constrainer.computeParagraphLevelConstraints(textWrapStyle);
        if (constrainedLineWidths)
            inlineLayoutState.setAvailableLineWidthOverride({ *constrainedLineWidths });
    }

    if (TextOnlySimpleLineBuilder::isEligibleForSimplifiedTextOnlyInlineLayoutByContent(inlineItems, inlineLayoutState.placedFloats()) && TextOnlySimpleLineBuilder::isEligibleForSimplifiedInlineLayoutByStyle(root())) {
        auto simplifiedLineBuilder = makeUniqueRef<TextOnlySimpleLineBuilder>(*this, root(), constraints.horizontal(), inlineItemList);
        return lineLayout(simplifiedLineBuilder, inlineItemList, needsLayoutRange, previousLine, constraints, lineDamage);
    }
    if (RangeBasedLineBuilder::isEligibleForRangeInlineLayout(*this, needsLayoutRange, inlineItems, inlineLayoutState.placedFloats())) {
        auto rangeBasedLineBuilder = makeUniqueRef<RangeBasedLineBuilder>(*this, constraints.horizontal(), inlineItems);
        return lineLayout(rangeBasedLineBuilder, inlineItemList, needsLayoutRange, previousLine, constraints, lineDamage);
    }
    auto lineBuilder = makeUniqueRef<LineBuilder>(*this, constraints.horizontal(), inlineItemList, inlineContentCache().textSpacingContext());
    return lineLayout(lineBuilder, inlineItemList, needsLayoutRange, previousLine, constraints, lineDamage);
}

std::pair<LayoutUnit, LayoutUnit> InlineFormattingContext::minimumMaximumContentSize(InlineDamage* lineDamage)
{
    auto& inlineContentCache = this->inlineContentCache();
    auto minimumContentSize = inlineContentCache.minimumContentSize();
    auto maximumContentSize = inlineContentCache.maximumContentSize();

    if (minimumContentSize && maximumContentSize)
        return { ceiledLayoutUnit(*minimumContentSize), ceiledLayoutUnit(*maximumContentSize) };

    rebuildInlineItemListIfNeeded(lineDamage);
    auto& inlineItems = inlineContentCache.inlineItems();

    if (!isEmptyInlineContent(inlineItems.content())) {
        auto intrinsicWidthHandler = IntrinsicWidthHandler { *this, inlineItems };

        if (!minimumContentSize)
            minimumContentSize = intrinsicWidthHandler.minimumContentSize();
        if (!maximumContentSize) {
            maximumContentSize = intrinsicWidthHandler.maximumContentSize();
            if (intrinsicWidthHandler.maximumIntrinsicWidthLineContent())
                inlineContentCache.setMaximumIntrinsicWidthLineContent(WTFMove(*intrinsicWidthHandler.maximumIntrinsicWidthLineContent()));
        }
    } else {
        minimumContentSize = minimumContentSize.value_or(0.f);
        maximumContentSize = maximumContentSize.value_or(0.f);
    }
#ifndef NDEBUG
    // FIXME: "Nominally, the smallest size a box could take that doesn’t lead to overflow that could be avoided by choosing a larger size.
    // Formally, the size of the box when sized under a min-content constraint"
    // 'nominally' seems to overrule 'formally' when inline content has negative text indent.
    // This also undermines the idea of computing min/max values independently.
    if (*minimumContentSize > *maximumContentSize) {
        auto hasNegativeImplicitMargin = [](auto& style) {
            auto textIndentFixedLength = style.textIndent().length.tryFixed();
            return (textIndentFixedLength && textIndentFixedLength->isNegative()) || style.usedWordSpacing() < 0 || style.usedLetterSpacing() < 0;
        };
        auto contentHasNegativeImplicitMargin = hasNegativeImplicitMargin(root().style());
        if (!contentHasNegativeImplicitMargin) {
            for (auto& layoutBox : descendantsOfType<Box>(root())) {
                contentHasNegativeImplicitMargin = hasNegativeImplicitMargin(layoutBox.style());
                if (contentHasNegativeImplicitMargin)
                    break;
            }
        }
        ASSERT(contentHasNegativeImplicitMargin);
    }
#endif
    minimumContentSize = std::min(*minimumContentSize, *maximumContentSize);

    inlineContentCache.setMinimumContentSize(*minimumContentSize);
    inlineContentCache.setMaximumContentSize(*maximumContentSize);
    return { ceiledLayoutUnit(*minimumContentSize), ceiledLayoutUnit(*maximumContentSize) };
}

LayoutUnit InlineFormattingContext::minimumContentSize(InlineDamage* lineDamage)
{
    auto& inlineContentCache = this->inlineContentCache();
    if (inlineContentCache.minimumContentSize())
        return ceiledLayoutUnit(*inlineContentCache.minimumContentSize());

    rebuildInlineItemListIfNeeded(lineDamage);
    auto& inlineItems = inlineContentCache.inlineItems();
    auto minimumContentSize = InlineLayoutUnit { };
    if (!isEmptyInlineContent(inlineItems.content()))
        minimumContentSize = IntrinsicWidthHandler { *this, inlineItems }.minimumContentSize();
    inlineContentCache.setMinimumContentSize(minimumContentSize);
    return ceiledLayoutUnit(minimumContentSize);
}

LayoutUnit InlineFormattingContext::maximumContentSize(InlineDamage* lineDamage)
{
    auto& inlineContentCache = this->inlineContentCache();
    if (inlineContentCache.maximumContentSize())
        return ceiledLayoutUnit(*inlineContentCache.maximumContentSize());

    rebuildInlineItemListIfNeeded(lineDamage);
    auto& inlineItems = inlineContentCache.inlineItems();
    auto maximumContentSize = InlineLayoutUnit { };
    if (!isEmptyInlineContent(inlineItems.content())) {
        auto intrinsicWidthHandler = IntrinsicWidthHandler { *this, inlineItems };

        maximumContentSize = intrinsicWidthHandler.maximumContentSize();
        if (intrinsicWidthHandler.maximumIntrinsicWidthLineContent())
            inlineContentCache.setMaximumIntrinsicWidthLineContent(WTFMove(*intrinsicWidthHandler.maximumIntrinsicWidthLineContent()));
    }
    inlineContentCache.setMaximumContentSize(maximumContentSize);
    return ceiledLayoutUnit(maximumContentSize);
}

static bool mayExitFromPartialLayout(const InlineDamage& lineDamage, size_t lineIndex, const InlineDisplay::Boxes& newContent)
{
    if (lineDamage.layoutStartPosition()->lineIndex == lineIndex) {
        // Never stop at the damaged line. Adding trailing overflowing content could easily produce the
        // same set of display boxes for the first damaged line.
        return false;
    }
    auto trailingContentFromPreviousLayout = lineDamage.trailingContentForLine(lineIndex);
    return trailingContentFromPreviousLayout ? (!newContent.isEmpty() && *trailingContentFromPreviousLayout == newContent.last()) : false;
}

static inline void handleAfterSideMargin(BlockLayoutState::MarginState& marginState, InlineDisplay::Content& displayContent, size_t partialContentLineOffset)
{
    if (auto lineIndex = InlineDisplayLineBuilder::trailingLineWithBlockLevelBox(displayContent.boxes)) {
        InlineDisplayLineBuilder::adjustLineBlockAfterSideWithCollapsedMargin(marginState, *lineIndex - partialContentLineOffset, displayContent.lines);
        return;
    }
    marginState.canCollapseMarginAfterWithChildren = false;
}

InlineLayoutResult InlineFormattingContext::lineLayout(AbstractLineBuilder& lineBuilder, const InlineItemList& inlineItemList, InlineItemRange needsLayoutRange, std::optional<PreviousLine> previousLine, const ConstraintsForInlineContent& constraints, const InlineDamage* lineDamage)
{
    ASSERT(!needsLayoutRange.isEmpty());
    auto layoutResult = InlineLayoutResult { };
    auto& inlineLayoutState = layoutState();

    auto isPartialLayout = InlineInvalidation::mayOnlyNeedPartialLayout(lineDamage);
    ASSERT(isPartialLayout || !previousLine);

    if (!isPartialLayout && (createDisplayContentForLineFromCachedContent(constraints, layoutResult) || createDisplayContentForEmptyInlineContent(constraints, inlineItemList, layoutResult))) {
        layoutResult.range = InlineLayoutResult::Range::Full;
        handleAfterSideMargin(inlineLayoutState.parentBlockLayoutState().marginState(), layoutResult.displayContent, { });
        return layoutResult;
    }

    if (!needsLayoutRange.start)
        layoutResult.displayContent.boxes.reserveInitialCapacity(estimatedDisplayBoxSize(inlineItemList.size()));

    auto floatingContext = this->floatingContext();
    auto lineLogicalTop = InlineLayoutUnit { constraints.logicalTop() };
    auto previousLineEnd = std::optional<InlineItemPosition> { };
    auto leadingInlineItemPosition = needsLayoutRange.start;
    auto partialContentLineCount = previousLine ? (previousLine->lineIndex + 1) : 0lu;
    auto isFirstFormattedLineCandidate = !previousLine || !previousLine->lineIndex;
    while (true) {

        auto lineInitialRect = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, formattingUtils().initialLineHeight(!previousLine.has_value()) };
        auto lineInput = LineInput { { leadingInlineItemPosition, needsLayoutRange.end }, lineInitialRect };
        auto lineIndex = previousLine ? (previousLine->lineIndex + 1) : 0lu;

        auto lineLayoutResult = lineBuilder.layoutInlineContent(lineInput, previousLine, isFirstFormattedLineCandidate);
        auto lineBox = LineBoxBuilder { *this, lineLayoutResult }.build(lineIndex);
        inlineLayoutState.setLineCount(inlineLayoutState.lineCount() + (lineBox.hasContent() ? 1lu : 0lu));
        auto lineLogicalRect = createDisplayContentForInlineContent(lineBox, lineLayoutResult, constraints, layoutResult.displayContent);
        updateBoxGeometryForPlacedFloats(lineLayoutResult.floatContent.placedFloats);
        updateLayoutStateWithLineLayoutResult(lineLayoutResult, lineLogicalRect, floatingContext);

        auto lineContentEnd = lineLayoutResult.inlineItemRange.end;
        leadingInlineItemPosition = InlineFormattingUtils::leadingInlineItemPositionForNextLine(lineContentEnd, previousLineEnd, !lineLayoutResult.floatContent.hasIntrusiveFloat.isEmpty() || !lineLayoutResult.floatContent.placedFloats.isEmpty(), needsLayoutRange.end);

        auto isEndOfContent = leadingInlineItemPosition == needsLayoutRange.end && lineLayoutResult.floatContent.suspendedFloats.isEmpty();
        if (isEndOfContent) {
            layoutResult.range = !isPartialLayout ? InlineLayoutResult::Range::Full : InlineLayoutResult::Range::FullFromDamage;
            break;
        }
        if (isPartialLayout && mayExitFromPartialLayout(*lineDamage, lineIndex, layoutResult.displayContent.boxes)) {
            layoutResult.range = InlineLayoutResult::Range::PartialFromDamage;
            break;
        }

        if (formattingUtils().shouldDiscardRemainingContentInBlockDirection()) {
            resetBoxGeometriesForDiscardedContent({ leadingInlineItemPosition, needsLayoutRange.end }, lineLayoutResult.floatContent.suspendedFloats);
            layoutResult.range = !isPartialLayout ? InlineLayoutResult::Range::Full : InlineLayoutResult::Range::FullFromDamage;
            layoutResult.didDiscardContent = true;
            break;
        }

        previousLine = PreviousLine { lineIndex, lineLayoutResult.contentGeometry.trailingOverflowingContentWidth, lineLayoutResult.endsWithLineBreak(), lineLayoutResult.directionality.inlineBaseDirection, WTFMove(lineLayoutResult.floatContent.suspendedFloats) };
        previousLineEnd = lineContentEnd;
        isFirstFormattedLineCandidate &= !lineLayoutResult.hasInflowContent();
        lineLogicalTop = formattingUtils().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext);
    }
    InlineDisplayLineBuilder::addLegacyLineClampTrailingLinkBoxIfApplicable(*this, inlineLayoutState, layoutResult.displayContent);
    handleAfterSideMargin(inlineLayoutState.parentBlockLayoutState().marginState(), layoutResult.displayContent, partialContentLineCount);

    return layoutResult;
}

void InlineFormattingContext::layoutFloatContentOnly(const ConstraintsForInlineContent& constraints)
{
    ASSERT(!root().hasInFlowChild());

    auto& inlineContentCache = this->inlineContentCache();
    auto floatingContext = this->floatingContext();
    auto& placedFloats = layoutState().placedFloats();

    InlineItemsBuilder { inlineContentCache, root(), m_globalLayoutState.securityOrigin() }.build({ });

    for (auto& inlineItem : inlineContentCache.inlineItems().content()) {
        if (inlineItem.isFloat()) {
            auto& floatBox = inlineItem.layoutBox();

            integrationUtils().layoutWithFormattingContextForBox(downcast<ElementBox>(floatBox));

            auto& floatBoxGeometry = geometryForBox(floatBox);
            auto staticPosition = LayoutPoint { constraints.horizontal().logicalLeft, constraints.logicalTop() };
            staticPosition.move(floatBoxGeometry.marginStart(), floatBoxGeometry.marginBefore());
            floatBoxGeometry.setTopLeft(staticPosition);

            auto floatBoxTopLeft = floatingContext.positionForFloat(floatBox, floatBoxGeometry, constraints.horizontal());
            floatBoxGeometry.setTopLeft(floatBoxTopLeft);
            placedFloats.add(floatingContext.makeFloatItem(floatBox, floatBoxGeometry));
            continue;
        }
        ASSERT_NOT_REACHED();
    }
}

void InlineFormattingContext::updateLayoutStateWithLineLayoutResult(const LineLayoutResult& lineLayoutResult, const InlineRect& lineLogicalRect, const FloatingContext& floatingContext)
{
    auto& layoutState = this->layoutState();
    if (auto firstLineGap = lineLayoutResult.lineGeometry.initialLetterClearGap) {
        ASSERT(!layoutState.clearGapBeforeFirstLine());
        layoutState.setClearGapBeforeFirstLine(*firstLineGap);
    }

    if (lineLayoutResult.isFirstLast.isLastLineWithInlineContent)
        layoutState.setClearGapAfterLastLine(formattingUtils().logicalTopForNextLine(lineLayoutResult, lineLogicalRect, floatingContext) - lineLogicalRect.bottom());

    lineLayoutResult.endsWithHyphen() ? layoutState.incrementSuccessiveHyphenatedLineCount() : layoutState.resetSuccessiveHyphenatedLineCount();
    layoutState.setFirstLineStartTrimForInitialLetter(lineLayoutResult.firstLineStartTrim);

    auto updateBlockBeforeMargin = [&] {
        auto& marginState = layoutState.parentBlockLayoutState().marginState();
        if (marginState.atBeforeSideOfBlock && lineLayoutResult.hasInflowContent())
            marginState.resetBeforeSideOfBlock();
        if (lineLayoutResult.hasInlineContent()) {
            // FIXME: This works as long as blocks are wrapped in anonymous blocks so
            // that a block can never be followed by another block here.
            marginState.resetMarginValues();
        }
    };
    updateBlockBeforeMargin();
}

void InlineFormattingContext::updateBoxGeometryForPlacedFloats(const LineLayoutResult::PlacedFloatList& placedFloats)
{
    for (auto& floatItem : placedFloats) {
        if (!floatItem.layoutBox()) {
            ASSERT_NOT_REACHED();
            // We should not be placing intrusive floats coming from parent BFC.
            continue;
        }
        auto& boxGeometry = geometryForBox(*floatItem.layoutBox());
        auto usedGeometry = floatItem.boxGeometry();
        boxGeometry.setTopLeft(BoxGeometry::borderBoxTopLeft(usedGeometry));
        // Adopt trimmed inline direction margin.
        boxGeometry.setHorizontalMargin(usedGeometry.horizontalMargin());
    }
}

InlineRect InlineFormattingContext::createDisplayContentForInlineContent(const LineBox& lineBox, const LineLayoutResult& lineLayoutResult, const ConstraintsForInlineContent& constraints, InlineDisplay::Content& displayContent)
{
    auto& inlineLayoutState = layoutState();
    auto lineClamp = inlineLayoutState.parentBlockLayoutState().lineClamp();
    // Eligible lines from nested block containers are already included (see layoutWithFormattingContextForBlockInInline).
    auto numberOfLinesWithInlineContent = inlineLayoutState.lineCountWithInlineContentIncludingNestedBlocks() + (lineLayoutResult.hasInlineContent() ? 1 : 0);
    auto numberOfVisibleLinesAllowed = lineClamp ? std::make_optional(lineClamp->maximumLines) : std::nullopt;

    auto lineIsFullyTruncatedInBlockDirection = numberOfVisibleLinesAllowed ? numberOfLinesWithInlineContent > *numberOfVisibleLinesAllowed : false;
    auto displayLine = InlineDisplayLineBuilder { *this, constraints }.build(lineLayoutResult, lineBox, lineIsFullyTruncatedInBlockDirection);
    auto boxes = InlineDisplayContentBuilder { *this, constraints, lineBox, displayLine }.build(lineLayoutResult);
    displayLine.setBoxCount(boxes.size());

    auto addTrailingEllipsisIfApplicable = [&] {
        if (lineLayoutResult.hasBlockContent()) {
            // When a block line is clamped, its content gets clamped and not this line itself.
            return;
        }
        auto isLegacyLineClamp = lineClamp && lineClamp->isLegacy;
        auto truncationPolicy = InlineFormattingUtils::lineEndingTruncationPolicy(root().style(), numberOfLinesWithInlineContent, numberOfVisibleLinesAllowed, lineBox.hasContent());
        InlineDisplayLineBuilder::applyEllipsisIfNeeded(truncationPolicy, displayLine, boxes, isLegacyLineClamp);
        auto lineHasLegacyLineClamp = isLegacyLineClamp && displayLine.hasEllipsis() && truncationPolicy == LineEndingTruncationPolicy::WhenContentOverflowsInBlockDirection;
        if (lineHasLegacyLineClamp)
            inlineLayoutState.setLegacyClampedLineIndex(lineBox.lineIndex());
    };
    addTrailingEllipsisIfApplicable();

    displayContent.boxes.appendVector(WTFMove(boxes));
    displayContent.lines.append(displayLine);
    inlineLayoutState.setLineCountWithInlineContentIncludingNestedBlocks(numberOfLinesWithInlineContent);
    return InlineFormattingUtils::flipVisualRectToLogicalForWritingMode(displayContent.lines.last().lineBoxRect(), root().writingMode());
}

void InlineFormattingContext::resetBoxGeometriesForDiscardedContent(const InlineItemRange& discardedRange, const LineLayoutResult::SuspendedFloatList& suspendedFloats)
{
    if (discardedRange.isEmpty() && suspendedFloats.isEmpty())
        return;

    auto& inlineItemList = inlineContentCache().inlineItems().content();
    for (auto index = discardedRange.startIndex(); index < discardedRange.endIndex(); ++index) {
        auto& inlineItem = inlineItemList[index];
        auto hasBoxGeometry = inlineItem.isAtomicInlineBox() || inlineItem.isFloat() || inlineItem.isHardLineBreak() || inlineItem.isInlineBoxStart() || inlineItem.isOpaque();
        if (!hasBoxGeometry)
            continue;
        geometryForBox(inlineItem.layoutBox()).reset();
    }

    for (auto* floatBox : suspendedFloats)
        geometryForBox(*floatBox).reset();
}

bool InlineFormattingContext::createDisplayContentForLineFromCachedContent(const ConstraintsForInlineContent& constraints, InlineLayoutResult& layoutResult)
{
    auto& inlineContentCache = this->inlineContentCache();

    if (!inlineContentCache.maximumIntrinsicWidthLineContent() || !inlineContentCache.maximumContentSize())
        return false;

    auto horizontalAvailableSpace = constraints.horizontal().logicalWidth;
    if (*inlineContentCache.maximumContentSize() > horizontalAvailableSpace) {
        inlineContentCache.clearMaximumIntrinsicWidthLineContent();
        return false;
    }
    if (!layoutState().placedFloats().isEmpty()) {
        inlineContentCache.clearMaximumIntrinsicWidthLineContent();
        return false;
    }

    auto& lineContent = *inlineContentCache.maximumIntrinsicWidthLineContent();
    auto restoreTrimmedTrailingWhitespaceIfApplicable = [&]() -> std::optional<bool> {
        // Special 'line-break: after-white-space' behavior where min/max width trims trailing whitespace, while
        // layout should preserve _overflowing_ trailing whitespace.
        if (root().style().lineBreak() != LineBreak::AfterWhiteSpace || !lineContent.trimmedTrailingWhitespaceWidth)
            return { };
        if (ceiledLayoutUnit(lineContent.contentGeometry.logicalWidth) + LayoutUnit::epsilon() <= horizontalAvailableSpace)
            return { };
        if (!Line::restoreTrimmedTrailingWhitespace(lineContent.trimmedTrailingWhitespaceWidth, lineContent.runs, lineContent.inlineItemRange, inlineContentCache.inlineItems().content())) {
            ASSERT_NOT_REACHED();
            return false;
        }
        lineContent.contentGeometry.logicalWidth += lineContent.trimmedTrailingWhitespaceWidth;
        lineContent.contentGeometry.logicalRightIncludingNegativeMargin += lineContent.trimmedTrailingWhitespaceWidth;
        lineContent.trimmedTrailingWhitespaceWidth = { };
        return true;
    };
    auto successfullyTrimmed = restoreTrimmedTrailingWhitespaceIfApplicable();
    if (successfullyTrimmed && !*successfullyTrimmed) {
        inlineContentCache.clearMaximumIntrinsicWidthLineContent();
        return false;
    }

    lineContent.lineGeometry.logicalTopLeft = { constraints.horizontal().logicalLeft, constraints.logicalTop() };
    lineContent.lineGeometry.logicalWidth = constraints.horizontal().logicalWidth;
    lineContent.contentGeometry.logicalLeft = InlineFormattingUtils::horizontalAlignmentOffset(root().style(), lineContent.contentGeometry.logicalWidth, lineContent.lineGeometry.logicalWidth, lineContent.hangingContent.logicalWidth, true);
    auto lineBox = LineBoxBuilder { *this, lineContent }.build({ });
    createDisplayContentForInlineContent(lineBox, lineContent, constraints, layoutResult.displayContent);
    return true;
}

bool InlineFormattingContext::createDisplayContentForEmptyInlineContent(const ConstraintsForInlineContent& constraints, const InlineItemList& inlineItemList, InlineLayoutResult& layoutResult)
{
    if (!isEmptyInlineContent(inlineItemList))
        return false;

    auto emptyLineBreakingResult =  LineLayoutResult { };
    emptyLineBreakingResult.lineGeometry = { { constraints.horizontal().logicalLeft, constraints.logicalTop() }, { constraints.horizontal().logicalWidth } };
    auto lineBox = LineBoxBuilder { *this, emptyLineBreakingResult }.build({ });
    createDisplayContentForInlineContent(lineBox, emptyLineBreakingResult, constraints, layoutResult.displayContent);
    return true;
}

void InlineFormattingContext::initializeInlineLayoutState(const LayoutState& globalLayoutState)
{
    auto& inlineLayoutState = layoutState();

    if (auto limitLinesValue = root().style().hyphenateLimitLines().tryValue())
        inlineLayoutState.setHyphenationLimitLines(limitLinesValue->value);
    // FIXME: Remove when IFC takes care of running layout on inline-blocks.
    inlineLayoutState.setShouldNotSynthesizeInlineBlockBaseline();
    if (globalLayoutState.inStandardsMode())
        inlineLayoutState.setInStandardsMode();
    if (globalLayoutState.isTextShapingAcrossInlineBoxesEnabled())
        inlineLayoutState.setShouldShapeTextAcrossInlineBoxes();
}

#if ASSERT_ENABLED
static inline bool isOkToAccessBoxGeometry(const Box& layoutBox, const ElementBox& rootBlockContainer, std::optional<InlineFormattingContext::EscapeReason> escapeReason)
{
    if (escapeReason == InlineFormattingContext::EscapeReason::InkOverflowNeedsInitialContiningBlockForStrokeWidth && is<InitialContainingBlock>(layoutBox))
        return true;
    // This is the non-escape case of accessing a box's geometry information within the same formatting context when computing static position for out-of-flow boxes.
    if (layoutBox.isOutOfFlowPositioned())
        return true;
    auto containingBlock = [&]() -> const Box* {
        for (auto* ancestor = &layoutBox.parent(); !is<InitialContainingBlock>(*ancestor); ancestor = &ancestor->parent()) {
            if (ancestor->isContainingBlockForInFlow())
                return ancestor;
        }
        return nullptr;
    };
    // This is the non-escape case of accessing a box's geometry information within the same formatting context.
    return containingBlock() == &rootBlockContainer;
};
#endif

const BoxGeometry& InlineFormattingContext::geometryForBox(const Box& layoutBox, std::optional<EscapeReason> escapeReason) const
{
    ASSERT_UNUSED(escapeReason, isOkToAccessBoxGeometry(layoutBox, root(), escapeReason));
    return m_globalLayoutState.geometryForBox(layoutBox);
}

BoxGeometry& InlineFormattingContext::geometryForBox(const Box& layoutBox)
{
    ASSERT(isOkToAccessBoxGeometry(layoutBox, root(), { }));
    return m_globalLayoutState.ensureGeometryForBox(layoutBox);
}

void InlineFormattingContext::rebuildInlineItemListIfNeeded(InlineDamage* lineDamage)
{
    auto& inlineContentCache = this->inlineContentCache();
    auto inlineItemListNeedsUpdate = inlineContentCache.inlineItems().isEmpty() || (lineDamage && lineDamage->isInlineItemListDirty());
    if (!inlineItemListNeedsUpdate)
        return;

    auto startPositionForInlineItemsBuilding = [&]() -> InlineItemPosition {
        if (!lineDamage) {
            ASSERT(inlineContentCache.inlineItems().isEmpty());
            return { };
        }
        if (auto startPosition = lineDamage->layoutStartPosition()) {
            if (lineDamage->reasons().contains(InlineDamage::Reason::Pagination)) {
                // FIXME: We don't support partial rebuild with certain types of content. Let's just re-collect inline items.
                return { };
            }
            return startPosition->inlineItemPosition;
        }
        // Unsupported damage. Need to run full build/layout.
        return { };
    };
    InlineItemsBuilder { inlineContentCache, root(), m_globalLayoutState.securityOrigin() }.build(startPositionForInlineItemsBuilding());
    if (lineDamage)
        lineDamage->setInlineItemListClean();
    inlineContentCache.clearMaximumIntrinsicWidthLineContent();
}

}
}

