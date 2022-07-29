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
#include "InlineDamage.h"
#include "InlineDisplayBox.h"
#include "InlineDisplayContentBuilder.h"
#include "InlineDisplayLineBuilder.h"
#include "InlineFormattingState.h"
#include "InlineItemsBuilder.h"
#include "InlineLineBox.h"
#include "InlineLineBoxBuilder.h"
#include "InlineTextItem.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
#include "LayoutLineBreakBox.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "Logging.h"
#include "TextUtil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const ContainerBox& formattingContextRoot, InlineFormattingState& formattingState, const InlineDamage* lineDamage)
    : FormattingContext(formattingContextRoot, formattingState)
    , m_lineDamage(lineDamage)
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

void InlineFormattingContext::layoutInFlowContent(const ConstraintsForInFlowContent& constraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    ASSERT(root().hasInFlowOrFloatingChild());

    invalidateFormattingState();
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
                        formattingContext->layoutInFlowContent(formattingGeometry().constraintsForInFlowContent(formattingRoot));
                    computeHeightAndMargin(formattingRoot, constraints.horizontal());
                    formattingContext->layoutOutOfFlowContent(formattingGeometry().constraintsForOutOfFlowContent(formattingRoot));
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
            // Text wrapper boxes (anonymous inline level boxes) don't have box geometries (they only generate boxes).
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
    InlineDisplayContentBuilder::computeIsFirstIsLastBoxForInlineContent(formattingState().boxes());
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::layoutInFlowContentForIntegration(const ConstraintsForInFlowContent& constraints)
{
    invalidateFormattingState();
    collectContentIfNeeded();
    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
    computeStaticPositionForOutOfFlowContent(formattingState().outOfFlowBoxes());
    InlineDisplayContentBuilder::computeIsFirstIsLastBoxForInlineContent(formattingState().boxes());
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraintsForIntegration()
{
    if (formattingState().intrinsicWidthConstraints())
        return *formattingState().intrinsicWidthConstraints();

    collectContentIfNeeded();

    auto constraints = IntrinsicWidthConstraints {
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Minimum)),
        ceiledLayoutUnit(computedIntrinsicWidthForConstraint(IntrinsicWidthMode::Maximum))
    };
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
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
    auto top = LayoutUnit { lines.first().top() };
    auto bottom = LayoutUnit { lines.last().bottom() + formattingState().clearGapAfterLastLine() };

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
    formattingState.boxes().reserveInitialCapacity(formattingState.inlineItems().size());
    InlineLayoutUnit lineLogicalTop = constraints.logicalTop();
    auto previousLine = std::optional<LineBuilder::PreviousLine> { };
    auto& floatingState = formattingState.floatingState();
    auto floatingContext = FloatingContext { *this, floatingState };

    auto lineBuilder = LineBuilder { *this, floatingState, constraints.horizontal(), inlineItems };
    while (!needsLayoutRange.isEmpty()) {
        auto initialLineHeight = [&]() -> InlineLayoutUnit {
            if (layoutState().inStandardsMode())
                return root().style().computedLineHeight();
            return formattingQuirks().initialLineHeight();
        }();
        auto initialLineConstraints = InlineRect { lineLogicalTop, constraints.horizontal().logicalLeft, constraints.horizontal().logicalWidth, initialLineHeight };
        auto lineContent = lineBuilder.layoutInlineContent(needsLayoutRange, initialLineConstraints, previousLine);
        auto lineLogicalRect = computeGeometryForLineContent(lineContent);

        auto lineContentRange = lineContent.inlineItemRange;
        if (!lineContentRange.isEmpty()) {
            ASSERT(needsLayoutRange.start < lineContentRange.end);
            lineLogicalTop = formattingGeometry().logicalTopForNextLine(lineContent, lineLogicalRect.bottom(), floatingContext);
            if (lineContent.isLastLineWithInlineContent) {
                // The final content height of this inline formatting context should include the cleared floats as well.
                formattingState.setClearGapAfterLastLine(lineLogicalTop - lineLogicalRect.bottom());
            }
            // When the trailing content is partial, we need to reuse the last InlineTextItem.
            auto lastInlineItemNeedsPartialLayout = lineContent.partialOverflowingContent.has_value();
            if (lastInlineItemNeedsPartialLayout) {
                auto lineLayoutHasAdvanced = !previousLine
                    || lineContentRange.end > previousLine->range.end
                    || (previousLine->partialOverflowingContent && previousLine->partialOverflowingContent->length > lineContent.partialOverflowingContent->length);
                if (!lineLayoutHasAdvanced) {
                    ASSERT_NOT_REACHED();
                    // Move over to the next run if we are stuck on this partial content (when the overflow content length remains the same).
                    // We certainly lose some content, but we would be busy looping otherwise.
                    lastInlineItemNeedsPartialLayout = false;
                }
            }
            needsLayoutRange.start = lastInlineItemNeedsPartialLayout ? lineContentRange.end - 1 : lineContentRange.end;
            previousLine = LineBuilder::PreviousLine { lineContentRange, !lineContent.runs.isEmpty() && lineContent.runs.last().isLineBreak(), lineContent.inlineBaseDirection, lineContent.partialOverflowingContent, lineContent.trailingOverflowingContentWidth };
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
    auto& boxes = formattingState.boxes();

    for (auto& outOfFlowBox : outOfFlowBoxes) {
        auto& outOfFlowGeometry = formattingState.boxGeometry(outOfFlowBox);
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
            // This is the first (non-float)child. Let's place it to the left of the first box.
            // <div><img style="position: absolute">text content</div>
            ASSERT(boxes.size());
            outOfFlowGeometry.setLogicalTopLeft({ boxes[0].left(), lines[0].top() });
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
            // Skip to the last box of this layout box. The last box's geometry is used to compute the out-of-flow box's static position.
            size_t lastBoxIndexOnPreviousLayoutBox = 0;
            for (; lastBoxIndexOnPreviousLayoutBox < boxes.size() && &boxes[lastBoxIndexOnPreviousLayoutBox].layoutBox() != previousContentSkippingFloats; ++lastBoxIndexOnPreviousLayoutBox) { }
            if (lastBoxIndexOnPreviousLayoutBox == boxes.size()) {
                // FIXME: In very rare cases, the previous box's content might have been completely collapsed and left us with no box.
                ASSERT_NOT_IMPLEMENTED_YET();
                return;
            }
            for (; lastBoxIndexOnPreviousLayoutBox < boxes.size() && &boxes[lastBoxIndexOnPreviousLayoutBox].layoutBox() == previousContentSkippingFloats; ++lastBoxIndexOnPreviousLayoutBox) { }
                --lastBoxIndexOnPreviousLayoutBox;
            // Let's check if the previous box is the last box on the current line and use the next box's left instead.
            auto& previousBox = boxes[lastBoxIndexOnPreviousLayoutBox];
            auto* nextBox = lastBoxIndexOnPreviousLayoutBox + 1 < boxes.size() ? &boxes[lastBoxIndexOnPreviousLayoutBox + 1] : nullptr;

            if (nextBox && nextBox->lineIndex() == previousBox.lineIndex()) {
                // Previous and next boxes are on the same line. The out-of-flow box is right at the previous box's logical right.
                // <div>text<img style="position: absolute">content</div>
                auto left = previousBox.right();
                if (previousContentSkippingFloats->isInlineBox() && !previousContentSkippingFloats->isAnonymous()) {
                    // <div>text<span><img style="position: absolute">content</span></div>
                    // or
                    // <div>text<span>content</span><img style="position: absolute"></div>
                    auto& inlineBoxBoxGeometry = geometryForBox(*previousContentSkippingFloats);
                    left = previousContentSkippingFloats == &outOfFlowBox->parent()
                        ? BoxGeometry::borderBoxLeft(inlineBoxBoxGeometry) + inlineBoxBoxGeometry.contentBoxLeft()
                        : BoxGeometry::borderBoxRect(inlineBoxBoxGeometry).right();
                }
                outOfFlowGeometry.setLogicalTopLeft({ left, lines[previousBox.lineIndex()].top() });
                return;
            }

            if (nextBox) {
                // The out of flow box is placed at the beginning of the next line (where the first box on the line is).
                // <div>text<br><img style="position: absolute"><img style="position: absolute">content</div>
                outOfFlowGeometry.setLogicalTopLeft({ nextBox->left(), lines[nextBox->lineIndex()].top() });
                return;
            }

            auto& lastLine = lines[previousBox.lineIndex()];
            // This out-of-flow box is the last box.
            // FIXME: Use isLineBreak instead to cover preserved new lines too.
            if (previousBox.layoutBox().isLineBreakBox()) {
                // <div>text<br><img style="position: absolute"><img style="position: absolute"></div>
                outOfFlowGeometry.setLogicalTopLeft({ lastLine.left(), lastLine.bottom() });
                return;
            }
            // FIXME: We may need to check if this box actually fits the last line and move it over to the "next" line.
            outOfFlowGeometry.setLogicalTopLeft({ previousBox.right(), lastLine.top() });
        };
        placeOutOfFlowBoxAfterPreviousInFlowBox();
    }
}

IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& layoutState = this->layoutState();
    if (formattingState().intrinsicWidthConstraints())
        return *formattingState().intrinsicWidthConstraints();

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

    auto maximumLineWidth = [&](auto intrinsicWidthMode) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& boxGeometry = formattingState().boxGeometry(*formattingRoot);
            auto contentWidth = (intrinsicWidthMode == IntrinsicWidthMode::Maximum ? intrinsicWidths->maximum : intrinsicWidths->minimum) - boxGeometry.horizontalMarginBorderAndPadding();
            boxGeometry.setContentBoxWidth(contentWidth);
        }
        return computedIntrinsicWidthForConstraint(intrinsicWidthMode);
    };

    auto minimumContentWidth = ceiledLayoutUnit(maximumLineWidth(IntrinsicWidthMode::Minimum));
    auto maximumContentWidth = ceiledLayoutUnit(maximumLineWidth(IntrinsicWidthMode::Maximum));
    auto constraints = formattingGeometry().constrainByMinMaxWidth(root(), { minimumContentWidth, maximumContentWidth });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(IntrinsicWidthMode intrinsicWidthMode) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto lineBuilder = LineBuilder { *this, inlineItems, intrinsicWidthMode };
    auto layoutRange = LineBuilder::InlineItemRange { 0 , inlineItems.size() };
    auto maximumLineWidth = InlineLayoutUnit { };
    auto maximumFloatWidth = LayoutUnit { };
    auto previousLine = std::optional<LineBuilder::PreviousLine> { };
    while (!layoutRange.isEmpty()) {
        auto intrinsicContent = lineBuilder.computedIntrinsicWidth(layoutRange, previousLine);
        maximumLineWidth = std::max(maximumLineWidth, intrinsicContent.logicalWidth);

        layoutRange.start = !intrinsicContent.partialOverflowingContent ? intrinsicContent.inlineItemRange.end : intrinsicContent.inlineItemRange.end - 1;
        previousLine = LineBuilder::PreviousLine { intrinsicContent.inlineItemRange, { }, { }, intrinsicContent.partialOverflowingContent };
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
    auto inlineItemsBuilder = InlineItemsBuilder { root(), formattingState };
    formattingState.addInlineItems(inlineItemsBuilder.build());
}

InlineRect InlineFormattingContext::computeGeometryForLineContent(const LineBuilder::LineContent& lineContent)
{
    auto& formattingState = this->formattingState();
    auto currentLineIndex = formattingState.lines().size();

    auto lineBox = LineBoxBuilder { *this }.build(lineContent, currentLineIndex);
    auto displayLine = InlineDisplayLineBuilder { *this }.build(lineContent, lineBox);
    formattingState.addBoxes(InlineDisplayContentBuilder { *this, formattingState }.build(lineContent, lineBox, displayLine, currentLineIndex));
    formattingState.addLineBox(WTFMove(lineBox));
    formattingState.addLine(displayLine);

    return InlineFormattingGeometry::flipVisualRectToLogicalForWritingMode(formattingState.lines().last().lineBoxRect(), root().style().writingMode());
}

void InlineFormattingContext::invalidateFormattingState()
{
    if (!m_lineDamage) {
        // Non-empty formatting state with no damage means we are trying to layout a clean tree.
        // FIXME: Add ASSERT(formattingState().inlineItems().isEmpty()) when all the codepaths are covered.
        formattingState().clearLineAndBoxes();
        return;
    }
    // FIXME: Lines and boxes are moved out to under Integration::InlineContent in the integration codepath, so clearLineAndBoxes is no-op.
    switch (m_lineDamage->type()) {
    case InlineDamage::Type::NeedsContentUpdateAndLineLayout:
        formattingState().clearInlineItems();
        FALLTHROUGH;
    case InlineDamage::Type::NeedsLineLayout:
        formattingState().clearLineAndBoxes();
        break;
    case InlineDamage::Type::NeedsVerticalAdjustment:
    case InlineDamage::Type::NeedsHorizontalAdjustment:
    default:
        ASSERT_NOT_IMPLEMENTED_YET();
        break;
    }
}

}
}

#endif
