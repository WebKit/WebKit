/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "FloatingContext.h"
#include "FontCascade.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineFormattingState.h"
#include "InlineLineBox.h"
#include "InlineLineBoxBuilder.h"
#include "InlineLineRun.h"
#include "InlineTextItem.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutLineBreakBox.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_inlineFormattingGeometry(*this)
    , m_inlineFormattingQuirks(*this)
{
}

static inline const Box* nextInlineLevelBoxToLayout(const Box& layoutBox, const ContainerBox& stayWithin)
{
    // Atomic inline-level boxes and floats are opaque boxes meaning that they are
    // responsible for their own content (do not need to descend into their subtrees).
    // Only inline boxes may have relevant descendant content.
    if (layoutBox.isInlineBox()) {
        if (is<ContainerBox>(layoutBox) && downcast<ContainerBox>(layoutBox).hasInFlowOrFloatingChild()) {
            // Anonymous inline text boxes/line breaks can't have descendant content by definition.
            ASSERT(!layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox());
            return downcast<ContainerBox>(layoutBox).firstInFlowOrFloatingChild();
        }
    }

    for (auto* nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &stayWithin; nextInPreOrder = &nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
}

void InlineFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const ConstraintsForInFlowContent& constraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    ASSERT(root().hasInFlowOrFloatingChild());

    invalidateFormattingState(invalidationState);
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, padding and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        ASSERT(layoutBox->isInlineLevelBox() || layoutBox->isFloatingPositioned());

        if (layoutBox->isAtomicInlineLevelBox() || layoutBox->isFloatingPositioned()) {
            // Inline-blocks, inline-tables and replaced elements (img, video) can be sized but not yet positioned.
            if (is<ContainerBox>(layoutBox) && layoutBox->establishesFormattingContext()) {
                ASSERT(layoutBox->isInlineBlockBox() || layoutBox->isInlineTableBox() || layoutBox->isFloatingPositioned());
                auto& formattingRoot = downcast<ContainerBox>(*layoutBox);
                computeBorderAndPadding(formattingRoot, constraints.horizontal());
                computeWidthAndMargin(formattingRoot, constraints.horizontal());

                if (formattingRoot.hasChild()) {
                    auto formattingContext = LayoutContext::createFormattingContext(formattingRoot, layoutState());
                    if (formattingRoot.hasInFlowOrFloatingChild())
                        formattingContext->layoutInFlowContent(invalidationState, formattingGeometry().constraintsForInFlowContent(formattingRoot));
                    computeHeightAndMargin(formattingRoot, constraints.horizontal());
                    formattingContext->layoutOutOfFlowContent(invalidationState, formattingGeometry().constraintsForOutOfFlowContent(formattingRoot));
                } else
                    computeHeightAndMargin(formattingRoot, constraints.horizontal());
            } else {
                // Replaced and other type of leaf atomic inline boxes.
                computeBorderAndPadding(*layoutBox, constraints.horizontal());
                computeWidthAndMargin(*layoutBox, constraints.horizontal());
                computeHeightAndMargin(*layoutBox, constraints.horizontal());
            }
        } else if (layoutBox->isLineBreakBox()) {
            auto& boxGeometry = formattingState().boxGeometry(*layoutBox);
            boxGeometry.setHorizontalMargin({ });
            boxGeometry.setBorder({ });
            boxGeometry.setPadding({ });
            boxGeometry.setContentBoxWidth({ });
            boxGeometry.setVerticalMargin({ });
        } else if (layoutBox->isInlineBox()) {
            // Text wrapper boxes (anonymous inline level boxes) don't have box geometries (they only generate runs).
            if (!layoutBox->isInlineTextBox()) {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and padding.
                computeBorderAndPadding(*layoutBox, constraints.horizontal());
                computeHorizontalMargin(*layoutBox, constraints.horizontal());
                formattingState().boxGeometry(*layoutBox).setVerticalMargin({ });
            }
        } else
            ASSERT_NOT_REACHED();

        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectContentIfNeeded();

    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
    computeStaticPositionForOutOfFlowContent(formattingState().outOfFlowBoxes());
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayoutForIntergration(InvalidationState& invalidationState, const ConstraintsForInFlowContent& constraints)
{
    invalidateFormattingState(invalidationState);
    collectContentIfNeeded();
    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
    computeStaticPositionForOutOfFlowContent(formattingState().outOfFlowBoxes());
}

LayoutUnit InlineFormattingContext::usedContentHeight() const
{
    // 10.6.7 'Auto' heights for block formatting context roots

    // If it only has inline-level children, the height is the distance between the top of the topmost line box and the bottom of the bottommost line box.

    // In addition, if the element has any floating descendants whose bottom margin edge is below the element's bottom content edge,
    // then the height is increased to include those edges. Only floats that participate in this block formatting context are taken
    // into account, e.g., floats inside absolutely positioned descendants or other floats are not.
    auto& lines = formattingState().lines();
    // Even empty content generates a line.
    ASSERT(!lines.isEmpty());
    auto top = LayoutUnit { lines.first().lineBoxLogicalRect().top() };
    auto bottom = LayoutUnit { lines.last().lineBoxLogicalRect().bottom() + formattingState().clearGapAfterLastLine() };

    auto floatingContext = FloatingContext { *this, formattingState().floatingState() };
    if (auto floatBottom = floatingContext.bottom()) {
        bottom = std::max(*floatBottom, bottom);
        top = std::min(*floatingContext.top(), top);
    }
    return bottom - top;
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, LineBuilder::InlineItemRange needsLayoutRange, const ConstraintsForInFlowContent& constraints)
{
    auto& formattingState = this->formattingState();
    formattingState.runs().reserveInitialCapacity(formattingState.inlineItems().size());
    InlineLayoutUnit lineLogicalTop = constraints.logicalTop();
    struct PreviousLine {
        LineBuilder::InlineItemRange range;
        size_t overflowContentLength { 0 };
        std::optional<InlineLayoutUnit> overflowLogicalWidth;
    };
    std::optional<PreviousLine> previousLine;
    auto& floatingState = formattingState.floatingState();
    auto floatingContext = FloatingContext { *this, floatingState };
    auto isFirstLine = formattingState.lines().isEmpty();

    auto lineBuilder = LineBuilder { *this, floatingState, constraints.horizontal(), inlineItems };
    while (!needsLayoutRange.isEmpty()) {
        // Turn previous line's overflow content length into the next line's leading content partial length.
        // "sp[<-line break->]lit_content" -> overflow length: 11 -> leading partial content length: 11.
        auto partialLeadingContentLength = previousLine ? previousLine->overflowContentLength : 0;
        auto leadingLogicalWidth = previousLine ? previousLine->overflowLogicalWidth : std::nullopt;
        auto initialLineHeight = [&]() -> InlineLayoutUnit {
            if (layoutState().inStandardsMode())
                return root().style().computedLineHeight();
            return formattingQuirks().initialLineHeight();
        }();
        auto initialLineConstraints = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, initialLineHeight };
        auto lineContent = lineBuilder.layoutInlineContent(needsLayoutRange, partialLeadingContentLength, leadingLogicalWidth, initialLineConstraints, isFirstLine);
        auto lineLogicalRect = computeGeometryForLineContent(lineContent);

        auto lineContentRange = lineContent.inlineItemRange;
        if (!lineContentRange.isEmpty()) {
            ASSERT(needsLayoutRange.start < lineContentRange.end);
            isFirstLine = false;
            lineLogicalTop = formattingGeometry().logicalTopForNextLine(lineContent, lineLogicalRect.bottom(), floatingContext);
            if (lineContent.isLastLineWithInlineContent) {
                // The final content height of this inline formatting context should include the cleared floats as well.
                formattingState.setClearGapAfterLastLine(lineLogicalTop - lineLogicalRect.bottom());
            }
            // When the trailing content is partial, we need to reuse the last InlineTextItem.
            auto lastInlineItemNeedsPartialLayout = lineContent.partialTrailingContentLength;
            if (lastInlineItemNeedsPartialLayout) {
                auto lineLayoutHasAdvanced = !previousLine
                    || lineContentRange.end > previousLine->range.end
                    || (previousLine->overflowContentLength && previousLine->overflowContentLength > lineContent.partialTrailingContentLength);
                if (!lineLayoutHasAdvanced) {
                    ASSERT_NOT_REACHED();
                    // Move over to the next run if we are stuck on this partial content (when the overflow content length remains the same).
                    // We certainly lose some content, but we would be busy looping otherwise.
                    lastInlineItemNeedsPartialLayout = false;
                }
            }
            needsLayoutRange.start = lastInlineItemNeedsPartialLayout ? lineContentRange.end - 1 : lineContentRange.end;
            previousLine = PreviousLine { lineContentRange, lineContent.partialTrailingContentLength, lineContent.overflowLogicalWidth };
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(lineContent.runs.isEmpty());
        ASSERT(lineContent.hasIntrusiveFloat);
        // Move the next line below the intrusive float(s).
        auto logicalTopCandidateForNextLine = [&] {
            auto lineBottomWithNoInlineContent = LayoutUnit { std::max(lineLogicalRect.bottom(), initialLineConstraints.bottom()) };
            auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalTop), lineBottomWithNoInlineContent);
            ASSERT(floatConstraints.left || floatConstraints.right);
            if (floatConstraints.left && floatConstraints.right) {
                // In case of left and right constraints, we need to pick the one that's closer to the current line.
                return std::min(floatConstraints.left->y, floatConstraints.right->y);
            }
            if (floatConstraints.left)
                return floatConstraints.left->y;
            if (floatConstraints.right)
                return floatConstraints.right->y;
            ASSERT_NOT_REACHED();
            return lineBottomWithNoInlineContent;
        };
        lineLogicalTop = logicalTopCandidateForNextLine();
    }
}

void InlineFormattingContext::computeStaticPositionForOutOfFlowContent(const FormattingState::OutOfFlowBoxList& outOfFlowBoxes)
{
    // This function computes the static position for out-of-flow content inside the inline formatting context.
    // As per spec, the static position of an out-of-flow box is computed as if the position was set to static.
    // However it does not mean that the out-of-flow box should be involved in the inline layout process.
    // Instead we figure out this static position after the inline layout by looking at the previous/next sibling (or parent) box's geometry and
    // place the out-of-flow box at the logical right position.
    auto& formattingState = this->formattingState();
    auto& lines = formattingState.lines();
    auto& runs = formattingState.runs();

    for (auto& outOfFlowBox : outOfFlowBoxes) {
        auto& outOfFlowGeometry = formattingState.boxGeometry(*outOfFlowBox);
        // Both previous float and out-of-flow boxes are skipped here. A series of adjoining out-of-flow boxes should all be placed
        // at the same static position (they don't affect next-sibling positions) and while floats do participate in the inline layout
        // their positions have already been taken into account during the inline layout.
        auto previousContentSkippingFloats = [&]() -> const Layout::Box* {
            auto* previousSibling = outOfFlowBox->previousSibling();
            for (; previousSibling && previousSibling->isFloatingPositioned(); previousSibling = previousSibling->previousSibling()) { }
            if (previousSibling)
                return previousSibling;
            // Parent is either the root here or another inline box (e.g. <span><img style="position: absolute"></span>)
            auto& parent = outOfFlowBox->parent();
            return &parent == &root() ? nullptr : &parent;
        }();

        if (!previousContentSkippingFloats) {
            // This is the first (non-float)child. Let's place it to the left of the first run.
            // <div><img style="position: absolute">text content</div>
            ASSERT(runs.size());
            outOfFlowGeometry.setLogicalTopLeft({ runs[0].logicalLeft(), lines[0].lineBoxLogicalRect().top() });
            continue;
        }

        if (previousContentSkippingFloats->isOutOfFlowPositioned()) {
            // Subsequent out-of-flow positioned boxes share the same static position.
            // <div>text content<img style="position: absolute"><img style="position: absolute"></div>
            outOfFlowGeometry.setLogicalTopLeft(BoxGeometry::borderBoxTopLeft(geometryForBox(*previousContentSkippingFloats)));
            continue;
        }

        ASSERT(previousContentSkippingFloats->isInFlow());
        auto placeOutOfFlowBoxAfterPreviousInFlowBox = [&] {
            // The out-of-flow box should be placed after this inflow box.
            // Skip to the last run of this layout box. The last run's geometry is used to compute the out-of-flow box's static position.
            size_t lastRunIndexOnPreviousLayoutBox = 0;
            for (; lastRunIndexOnPreviousLayoutBox < runs.size() && &runs[lastRunIndexOnPreviousLayoutBox].layoutBox() != previousContentSkippingFloats; ++lastRunIndexOnPreviousLayoutBox) { }
            if (lastRunIndexOnPreviousLayoutBox == runs.size()) {
                // FIXME: In very rare cases, the previous box's content might have been completely collapsed and left us with no run.
                ASSERT_NOT_IMPLEMENTED_YET();
                return;
            }
            for (; lastRunIndexOnPreviousLayoutBox < runs.size() && &runs[lastRunIndexOnPreviousLayoutBox].layoutBox() == previousContentSkippingFloats; ++lastRunIndexOnPreviousLayoutBox) { }
                --lastRunIndexOnPreviousLayoutBox;
            // Let's check if the previous run is the last run on the current line and use the next run's left instead.
            auto& previousRun = runs[lastRunIndexOnPreviousLayoutBox];
            auto* nextRun = lastRunIndexOnPreviousLayoutBox + 1 < runs.size() ? &runs[lastRunIndexOnPreviousLayoutBox + 1] : nullptr;

            if (nextRun && nextRun->lineIndex() == previousRun.lineIndex()) {
                // Previous and next runs are on the same line. The out-of-flow box is right at the previous run's logical right.
                // <div>text<img style="position: absolute">content</div>
                auto logicalLeft = previousRun.logicalRight();
                if (previousContentSkippingFloats->isInlineBox() && !previousContentSkippingFloats->isAnonymous()) {
                    // <div>text<span><img style="position: absolute">content</span></div>
                    // or
                    // <div>text<span>content</span><img style="position: absolute"></div>
                    auto& inlineBoxBoxGeometry = geometryForBox(*previousContentSkippingFloats);
                    logicalLeft = previousContentSkippingFloats == &outOfFlowBox->parent()
                        ? BoxGeometry::borderBoxLeft(inlineBoxBoxGeometry) + inlineBoxBoxGeometry.contentBoxLeft()
                        : BoxGeometry::borderBoxRect(inlineBoxBoxGeometry).right();
                }
                outOfFlowGeometry.setLogicalTopLeft({ logicalLeft, lines[previousRun.lineIndex()].lineBoxLogicalRect().top() });
                return;
            }

            if (nextRun) {
                // The out of flow box is placed at the beginning of the next line (where the first run on the line is).
                // <div>text<br><img style="position: absolute"><img style="position: absolute">content</div>
                outOfFlowGeometry.setLogicalTopLeft({ nextRun->logicalLeft(), lines[nextRun->lineIndex()].lineBoxLogicalRect().top() });
                return;
            }

            auto& lastLineLogicalRect = lines[previousRun.lineIndex()].lineBoxLogicalRect();
            // This out-of-flow box is the last box.
            // FIXME: Use isLineBreak instead to cover preserved new lines too.
            if (previousRun.layoutBox().isLineBreakBox()) {
                // <div>text<br><img style="position: absolute"><img style="position: absolute"></div>
                outOfFlowGeometry.setLogicalTopLeft({ lastLineLogicalRect.left(), lastLineLogicalRect.bottom() });
                return;
            }
            // FIXME: We may need to check if this box actually fits the last line and move it over to the "next" line.
            outOfFlowGeometry.setLogicalTopLeft({ previousRun.logicalRight(), lastLineLogicalRect.top() });
        };
        placeOutOfFlowBoxAfterPreviousInFlowBox();
    }
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& layoutState = this->layoutState();
    ASSERT(!formattingState().intrinsicWidthConstraints());

    if (!root().hasInFlowOrFloatingChild()) {
        auto constraints = formattingGeometry().constrainByMinMaxWidth(root(), { });
        formattingState().setIntrinsicWidthConstraints(constraints);
        return constraints;
    }

    Vector<const Box*> formattingContextRootList;
    auto horizontalConstraints = HorizontalConstraints { 0_lu, 0_lu };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // In order to compute the max/min widths, we need to compute margins, borders and padding for certain inline boxes first.
    while (layoutBox) {
        if (layoutBox->isInlineTextBox()) {
            layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
            continue;
        }
        if (layoutBox->isReplacedBox()) {
            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeWidthAndMargin(*layoutBox, horizontalConstraints);
        } else if (layoutBox->isFloatingPositioned() || layoutBox->isAtomicInlineLevelBox()) {
            ASSERT(layoutBox->establishesFormattingContext());
            formattingContextRootList.append(layoutBox);

            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeHorizontalMargin(*layoutBox, horizontalConstraints);
            computeIntrinsicWidthForFormattingRoot(*layoutBox);
        } else if (layoutBox->isInlineBox()) {
            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeHorizontalMargin(*layoutBox, horizontalConstraints);
        } else
            ASSERT_NOT_REACHED();
        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectContentIfNeeded();

    auto maximumLineWidth = [&](auto availableWidth) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& boxGeometry = formattingState().boxGeometry(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - boxGeometry.horizontalMarginBorderAndPadding();
            boxGeometry.setContentBoxWidth(contentWidth);
        }
        return computedIntrinsicWidthForConstraint(availableWidth);
    };

    auto minimumContentWidth = ceiledLayoutUnit(maximumLineWidth(0));
    auto maximumContentWidth = ceiledLayoutUnit(maximumLineWidth(maxInlineLayoutUnit()));
    auto constraints = formattingGeometry().constrainByMinMaxWidth(root(), { minimumContentWidth, maximumContentWidth });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(InlineLayoutUnit availableWidth) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto lineBuilder = LineBuilder { *this, inlineItems };
    auto layoutRange = LineBuilder::InlineItemRange { 0 , inlineItems.size() };
    auto maximumLineWidth = InlineLayoutUnit { };
    auto maximumFloatWidth = LayoutUnit { };
    while (!layoutRange.isEmpty()) {
        auto intrinsicContent = lineBuilder.computedIntrinsicWidth(layoutRange, availableWidth);
        layoutRange.start = intrinsicContent.inlineItemRange.end;
        maximumLineWidth = std::max(maximumLineWidth, intrinsicContent.logicalWidth);
        // FIXME: Add support for clear.
        for (auto* floatBox : intrinsicContent.floats)
            maximumFloatWidth += geometryForBox(*floatBox).marginBoxWidth();
    }
    return maximumLineWidth + maximumFloatWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());
    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = formattingGeometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else {
        auto hasInflowOrFloatingContent = is<ContainerBox>(formattingRoot) && downcast<ContainerBox>(formattingRoot).hasInFlowOrFloatingChild();
        // The intrinsic sizes of the size containment box are determined as if the element had no content.
        auto shouldIgnoreChildContent = formattingRoot.isSizeContainmentBox();
        if (hasInflowOrFloatingContent && !shouldIgnoreChildContent)
            constraints = LayoutContext::createFormattingContext(downcast<ContainerBox>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    }
    constraints = formattingGeometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto computedHorizontalMargin = formattingGeometry().computedHorizontalMargin(layoutBox, horizontalConstraints);
    formattingState().boxGeometry(layoutBox).setHorizontalMargin({ computedHorizontalMargin.start.value_or(0), computedHorizontalMargin.end.value_or(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](std::optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return formattingGeometry().floatingContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isInlineBlockBox())
            return formattingGeometry().inlineBlockContentWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isReplacedBox())
            return formattingGeometry().inlineReplacedContentWidthAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedWidth, { } });
        ASSERT_NOT_REACHED();
        return ContentWidthAndMargin { };
    };

    auto contentWidthAndMargin = compute({ });

    auto availableWidth = horizontalConstraints.logicalWidth;
    if (auto maxWidth = formattingGeometry().computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = formattingGeometry().computedMinWidth(layoutBox, availableWidth).value_or(0);
    auto minWidthAndMargin = compute(minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    boxGeometry.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](std::optional<LayoutUnit> usedHeight) {
        if (layoutBox.isFloatingPositioned())
            return formattingGeometry().floatingContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isInlineBlockBox())
            return formattingGeometry().inlineBlockContentHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isReplacedBox())
            return formattingGeometry().inlineReplacedContentHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedHeight });
        ASSERT_NOT_REACHED();
        return ContentHeightAndMargin { };
    };

    auto contentHeightAndMargin = compute({ });
    if (auto maxHeight = formattingGeometry().computedMaxHeight(layoutBox)) {
        auto maxHeightAndMargin = compute(maxHeight);
        if (contentHeightAndMargin.contentHeight > maxHeightAndMargin.contentHeight)
            contentHeightAndMargin = maxHeightAndMargin;
    }

    if (auto minHeight = formattingGeometry().computedMinHeight(layoutBox)) {
        auto minHeightAndMargin = compute(minHeight);
        if (contentHeightAndMargin.contentHeight < minHeightAndMargin.contentHeight)
            contentHeightAndMargin = minHeightAndMargin;
    }
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    boxGeometry.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin.before, contentHeightAndMargin.nonCollapsedMargin.after });
}

void InlineFormattingContext::collectContentIfNeeded()
{
    auto& formattingState = this->formattingState();
    if (!formattingState.inlineItems().isEmpty())
        return;
    // Traverse the tree and create inline items out of inline boxes and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [InlineBoxStart][InlineLevelBox][InlineBoxStart][InlineBoxEnd][InlineLevelBox][InlineBoxEnd]
    ASSERT(root().hasInFlowOrFloatingChild());
    LayoutQueue layoutQueue;
    layoutQueue.append(root().firstChild());
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            auto isInlineBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox() && !layoutBox.isOutOfFlowPositioned();
            if (!isInlineBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxStart });
            auto& inlineBoxWithInlineContent = downcast<ContainerBox>(layoutBox);
            if (!inlineBoxWithInlineContent.hasChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isOutOfFlowPositioned()) {
                // Let's not construct InlineItems for out-of-flow content as they don't participate in the inline layout.
                // However to be able to static positioning them, we need to compute their approximate positions.
                formattingState.addOutOfFlowBox(layoutBox);
            } else if (is<LineBreakBox>(layoutBox)) {
                auto& lineBreakBox = downcast<LineBreakBox>(layoutBox);
                formattingState.addInlineItem({ layoutBox, lineBreakBox.isOptional() ? InlineItem::Type::WordBreakOpportunity : InlineItem::Type::HardLineBreak });
            } else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isInlineTextBox()) {
                InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), downcast<InlineTextBox>(layoutBox));
            } else if (layoutBox.isInlineBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::InlineBoxEnd });
            else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

InlineRect InlineFormattingContext::computeGeometryForLineContent(const LineBuilder::LineContent& lineContent)
{
    auto& formattingState = this->formattingState();
    auto currentLineIndex = formattingState.lines().size();

    auto lineBoxAndGeometry = LineBoxBuilder(*this).build(lineContent);
    formattingState.addLineBox(WTFMove(lineBoxAndGeometry.lineBox));
    formattingState.addLine(lineBoxAndGeometry.lineGeometry);

    auto lineBoxLogicalRect = lineBoxAndGeometry.lineGeometry.lineBoxLogicalRect();

    InlineDisplayContentBuilder(root(), formattingState).build(lineContent, formattingState.lineBoxes().last(), lineBoxLogicalRect.topLeft(), currentLineIndex);
    return lineBoxLogicalRect;
}

void InlineFormattingContext::invalidateFormattingState(const InvalidationState&)
{
    // Find out what we need to invalidate. This is where we add some smarts to do partial line layout.
    // For now let's just clear the runs.
    formattingState().clearLineAndRuns();
    // FIXME: This is also where we would delete inline items if their content changed.
}

}
}

#endif
