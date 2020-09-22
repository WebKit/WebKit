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
#include "InlineFormattingState.h"
#include "InlineLineBox.h"
#include "InlineLineRun.h"
#include "InlineTextItem.h"
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutInlineTextBox.h"
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
                computeBorderAndPadding(formattingRoot, constraints.horizontal);
                computeWidthAndMargin(formattingRoot, constraints.horizontal);

                if (formattingRoot.hasChild()) {
                    auto formattingContext = LayoutContext::createFormattingContext(formattingRoot, layoutState());
                    if (formattingRoot.hasInFlowOrFloatingChild())
                        formattingContext->layoutInFlowContent(invalidationState, geometry().constraintsForInFlowContent(formattingRoot));
                    computeHeightAndMargin(formattingRoot, constraints.horizontal);
                    formattingContext->layoutOutOfFlowContent(invalidationState, geometry().constraintsForOutOfFlowContent(formattingRoot));
                } else
                    computeHeightAndMargin(formattingRoot, constraints.horizontal);
            } else {
                // Replaced and other type of leaf atomic inline boxes.
                computeBorderAndPadding(*layoutBox, constraints.horizontal);
                computeWidthAndMargin(*layoutBox, constraints.horizontal);
                computeHeightAndMargin(*layoutBox, constraints.horizontal);
            }
        } else if (layoutBox->isInlineBox()) {
            // Text wrapper boxes (anonymous inline level boxes) and <br>s don't have box geometries (they only generate runs).
            if (!layoutBox->isInlineTextBox() && !layoutBox->isLineBreakBox()) {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and padding.
                computeBorderAndPadding(*layoutBox, constraints.horizontal);
                computeHorizontalMargin(*layoutBox, constraints.horizontal);
                formattingState().boxGeometry(*layoutBox).setVerticalMargin({ });
            }
        } else
            ASSERT_NOT_REACHED();

        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectInlineContentIfNeeded();

    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, constraints);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, LineBuilder::InlineItemRange needsLayoutRange, const ConstraintsForInFlowContent& constraints)
{
    auto lineLogicalTop = constraints.vertical.logicalTop;
    struct PreviousLine {
        LineBuilder::InlineItemRange range;
        Optional<unsigned> overflowContentLength;
    };
    Optional<PreviousLine> previousLine;
    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    auto isFirstLine = formattingState().lines().isEmpty();

    auto lineBuilder = LineBuilder { *this, floatingContext, root(), inlineItems };
    while (!needsLayoutRange.isEmpty()) {
        // Turn previous line's overflow content length into the next line's leading content partial length.
        // "sp[<-line break->]lit_content" -> overflow length: 11 -> leading partial content length: 11.
        auto partialLeadingContentLength = previousLine ? previousLine->overflowContentLength : WTF::nullopt;
        auto initialLineConstraints = ConstraintsForInFlowContent { constraints.horizontal, { lineLogicalTop, makeOptional(toLayoutUnit(quirks().initialLineHeight())) } };
        auto lineContent = lineBuilder.layoutInlineContent(needsLayoutRange, partialLeadingContentLength, initialLineConstraints, isFirstLine);
        auto lineLogicalRect = computeGeometryForLineContent(lineContent, constraints.horizontal);

        auto lineContentRange = lineContent.inlineItemRange;
        if (!lineContentRange.isEmpty()) {
            ASSERT(needsLayoutRange.start < lineContentRange.end);
            isFirstLine = false;
            lineLogicalTop = lineLogicalRect.bottom();
            // When the trailing content is partial, we need to reuse the last InlineTextItem.
            auto lastInlineItemNeedsPartialLayout = lineContent.partialContent.hasValue();
            if (lastInlineItemNeedsPartialLayout) {
                ASSERT(lineContent.partialContent->overflowContentLength);
                auto lineLayoutHasAdvanced = !previousLine
                    || lineContentRange.end > previousLine->range.end
                    || (previousLine->overflowContentLength && *previousLine->overflowContentLength > lineContent.partialContent->overflowContentLength);
                if (!lineLayoutHasAdvanced) {
                    ASSERT_NOT_REACHED();
                    // Move over to the next run if we are stuck on this partial content (when the overflow content length remains the same).
                    // We certainly lose some content, but we would be busy looping otherwise.
                    lastInlineItemNeedsPartialLayout = false;
                }
            }
            needsLayoutRange.start = lastInlineItemNeedsPartialLayout ? lineContentRange.end - 1 : lineContentRange.end;
            previousLine = PreviousLine { lineContentRange, lineContent.partialContent ? makeOptional(lineContent.partialContent->overflowContentLength) : WTF::nullopt };
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(lineContent.runs.isEmpty());
        ASSERT(lineContent.hasIntrusiveFloat);
        // Move the next line below the intrusive float.
        auto floatConstraints = floatingContext.constraints(lineLogicalTop, toLayoutUnit(lineLogicalRect.bottom()));
        ASSERT(floatConstraints.left || floatConstraints.right);
        static auto inifitePoint = PointInContextRoot::max();
        // In case of left and right constraints, we need to pick the one that's closer to the current line.
        lineLogicalTop = std::min(floatConstraints.left.valueOr(inifitePoint).y, floatConstraints.right.valueOr(inifitePoint).y);
        ASSERT(lineLogicalTop < inifitePoint.y);
    }
}

FormattingContext::IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& layoutState = this->layoutState();
    ASSERT(!formattingState().intrinsicWidthConstraints());

    if (!root().hasInFlowOrFloatingChild()) {
        auto constraints = geometry().constrainByMinMaxWidth(root(), { });
        formattingState().setIntrinsicWidthConstraints(constraints);
        return constraints;
    }

    Vector<const Box*> formattingContextRootList;
    auto horizontalConstraints = HorizontalConstraints { 0_lu, 0_lu };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // In order to compute the max/min widths, we need to compute margins, borders and padding for certain inline boxes first.
    while (layoutBox) {
        if (layoutBox->isInlineTextBox() || layoutBox->isLineBreakBox()) {
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

    collectInlineContentIfNeeded();

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
    auto constraints = geometry().constrainByMinMaxWidth(root(), { minimumContentWidth, maximumContentWidth });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(InlineLayoutUnit availableWidth) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto maximumLineWidth = InlineLayoutUnit { };
    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    auto lineBuilder = LineBuilder { *this, floatingContext, root(), inlineItems };
    auto layoutRange = LineBuilder::InlineItemRange { 0 , inlineItems.size() };
    while (!layoutRange.isEmpty()) {
        auto intrinsicContent = lineBuilder.computedIntrinsicWidth(layoutRange, availableWidth);
        layoutRange.start = intrinsicContent.inlineItemRange.end;
        // FIXME: Use line logical left and right to take floats into account.
        maximumLineWidth = std::max(maximumLineWidth, intrinsicContent.logicalWidth);
    }
    return maximumLineWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());
    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = geometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else if (is<ContainerBox>(formattingRoot) && downcast<ContainerBox>(formattingRoot).hasInFlowOrFloatingChild())
        constraints = LayoutContext::createFormattingContext(downcast<ContainerBox>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    constraints = geometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, horizontalConstraints);
    formattingState().boxGeometry(layoutBox).setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedWidth) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockWidthAndMargin(layoutBox, horizontalConstraints, { usedWidth, { } });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedWidthAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedWidth, { } });
        ASSERT_NOT_REACHED();
        return ContentWidthAndMargin { };
    };

    auto contentWidthAndMargin = compute({ });

    auto availableWidth = horizontalConstraints.logicalWidth;
    if (auto maxWidth = geometry().computedMaxWidth(layoutBox, availableWidth)) {
        auto maxWidthAndMargin = compute(maxWidth);
        if (contentWidthAndMargin.contentWidth > maxWidthAndMargin.contentWidth)
            contentWidthAndMargin = maxWidthAndMargin;
    }

    auto minWidth = geometry().computedMinWidth(layoutBox, availableWidth).valueOr(0);
    auto minWidthAndMargin = compute(minWidth);
    if (contentWidthAndMargin.contentWidth < minWidthAndMargin.contentWidth)
        contentWidthAndMargin = minWidthAndMargin;

    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    boxGeometry.setHorizontalMargin({ contentWidthAndMargin.usedMargin.start, contentWidthAndMargin.usedMargin.end });
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto compute = [&](Optional<LayoutUnit> usedHeight) {
        if (layoutBox.isFloatingPositioned())
            return geometry().floatingHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isInlineBlockBox())
            return geometry().inlineBlockHeightAndMargin(layoutBox, horizontalConstraints, { usedHeight });
        if (layoutBox.isReplacedBox())
            return geometry().inlineReplacedHeightAndMargin(downcast<ReplacedBox>(layoutBox), horizontalConstraints, { }, { usedHeight });
        ASSERT_NOT_REACHED();
        return ContentHeightAndMargin { };
    };

    auto contentHeightAndMargin = compute({ });
    if (auto maxHeight = geometry().computedMaxHeight(layoutBox)) {
        auto maxHeightAndMargin = compute(maxHeight);
        if (contentHeightAndMargin.contentHeight > maxHeightAndMargin.contentHeight)
            contentHeightAndMargin = maxHeightAndMargin;
    }

    if (auto minHeight = geometry().computedMinHeight(layoutBox)) {
        auto minHeightAndMargin = compute(minHeight);
        if (contentHeightAndMargin.contentHeight < minHeightAndMargin.contentHeight)
            contentHeightAndMargin = minHeightAndMargin;
    }
    auto& boxGeometry = formattingState().boxGeometry(layoutBox);
    boxGeometry.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    boxGeometry.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin.before, contentHeightAndMargin.nonCollapsedMargin.after });
}

void InlineFormattingContext::collectInlineContentIfNeeded()
{
    auto& formattingState = this->formattingState();
    if (!formattingState.inlineItems().isEmpty())
        return;
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [ContainerStart][InlineBox][ContainerStart][ContainerEnd][InlineBox][ContainerEnd]
    ASSERT(root().hasInFlowOrFloatingChild());
    LayoutQueue layoutQueue;
    layoutQueue.append(root().firstInFlowOrFloatingChild());
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            auto isBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isInlineTextBox() && !layoutBox.isLineBreakBox();
            if (!isBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::ContainerStart });
            auto& inlineBoxWithInlineContent = downcast<ContainerBox>(layoutBox);
            if (!inlineBoxWithInlineContent.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isLineBreakBox()) {
                // FIXME: Treat <wbr> as a word break opportunity instead.
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::HardLineBreak });
            } else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isInlineTextBox()) {
                InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), downcast<InlineTextBox>(layoutBox));
            } else if (layoutBox.isInlineBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::ContainerEnd });
            else
                ASSERT_NOT_REACHED();

            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

InlineRect InlineFormattingContext::computeGeometryForLineContent(const LineBuilder::LineContent& lineContent, const HorizontalConstraints& horizontalConstraints)
{
    auto& formattingState = this->formattingState();
    auto geometry = this->geometry();

    formattingState.addLineBox(geometry.lineBoxForLineContent(lineContent));
    const auto& lineBox = formattingState.lineBoxes().last();

    auto lineRectAndLineBoxOffset = geometry.computedLineLogicalRect(lineBox, root().style(), lineContent);
    auto lineLogicalRect = lineRectAndLineBoxOffset.logicalRect;
    auto lineBoxVerticalOffset = lineRectAndLineBoxOffset.lineBoxVerticalOffset;

    auto updateFloatGeometry = [&] {
        if (lineContent.floats.isEmpty())
            return;
        auto floatingContext = FloatingContext { root(), *this, formattingState.floatingState() };
        // Move floats to their final position.
        for (const auto& floatCandidate : lineContent.floats) {
            auto& floatBox = floatCandidate.item->layoutBox();
            auto& boxGeometry = formattingState.boxGeometry(floatBox);
            // Set static position first.
            auto verticalStaticPosition = floatCandidate.isIntrusive ? lineLogicalRect.top() : lineLogicalRect.bottom();
            boxGeometry.setLogicalTopLeft({ lineLogicalRect.left(), verticalStaticPosition });
            // Float it.
            boxGeometry.setLogicalTopLeft(floatingContext.positionForFloat(floatBox, horizontalConstraints));
            floatingContext.append(floatBox);
        }
    };
    updateFloatGeometry();

    auto constructLineRuns = [&] {
        auto lineIndex = formattingState.lines().size();
        // Create the inline runs on the current line. This is mostly text and atomic inline runs.
        for (auto& lineRun : lineContent.runs) {
            if (lineRun.isText() || lineRun.isLineBreak())
                formattingState.addLineRun({ lineIndex, lineRun.layoutBox(), lineBox.logicalRectForTextRun(lineRun), lineRun.expansion(), lineRun.textContent() });
            else if (lineRun.isBox())
                formattingState.addLineRun({ lineIndex, lineRun.layoutBox(), lineBox.inlineBoxForLayoutBox(lineRun.layoutBox()).logicalRect(), lineRun.expansion(), { } });
        }
    };
    constructLineRuns();

    auto updateBoxGeometry = [&] {
        // Grab the inline boxes (even those that don't have associated layout boxes on the current line due to line wrapping)
        // and update their geometries.
        for (auto& inlineBox : lineBox.inlineBoxList()) {
            auto& layoutBox = inlineBox->layoutBox();
            if (&layoutBox == &root()) {
                // Ignore root inline box.
                continue;
            }
            auto& boxGeometry = formattingState.boxGeometry(layoutBox);
            auto logicalTopLeft = inlineBox->logicalRect().topLeft();

            logicalTopLeft.move({ }, lineBoxVerticalOffset);
            if (layoutBox.isInFlowPositioned())
                logicalTopLeft += geometry.inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);

            if (layoutBox.isAtomicInlineLevelBox()) {
                // Atomic inline boxes are all set. Their margin/border/content box geometries are already computed. We just have to position them here.
                boxGeometry.setLogicalTopLeft(toLayoutPoint(logicalTopLeft));
                continue;
            }
            auto marginBoxWidth = inlineBox->logicalWidth();
            auto contentBoxWidth = marginBoxWidth - (boxGeometry.marginStart() + boxGeometry.borderLeft() + boxGeometry.paddingLeft().valueOr(0));
            // Non-atomic inline level boxes may or may not be wrapped and have geometries on multiple lines.
            int previousLineIndex = formattingState.lineBoxes().size() - 2;
            auto isSpanningInlineBox = previousLineIndex > 0 && formattingState.lineBoxes()[previousLineIndex].containsInlineLevelBox(layoutBox);
            if (!isSpanningInlineBox) {
                // This box showed up on this line the first time.
                boxGeometry.setLogicalTopLeft(toLayoutPoint(logicalTopLeft));
                boxGeometry.setContentBoxWidth(toLayoutUnit(contentBoxWidth));
                boxGeometry.setContentBoxHeight(toLayoutUnit(inlineBox->logicalHeight()));
                continue;
            }
            // This is a just a simple box geometry for the line spanning inline box. getBoundingClientRect looks into each line boxes (will turn into fragmented boxes).
            boxGeometry.setLogicalLeft(std::min(boxGeometry.logicalLeft(), toLayoutUnit(logicalTopLeft.x())));
            boxGeometry.setContentBoxWidth(std::max(toLayoutUnit(contentBoxWidth), boxGeometry.contentBoxWidth()));
            boxGeometry.setContentBoxHeight(boxGeometry.contentBoxHeight() + toLayoutUnit(inlineBox->logicalHeight()));
        }
    };
    updateBoxGeometry();

    auto constructLineGeometry = [&] {
        auto lineBoxLogicalRect = InlineRect { lineLogicalRect.top() + lineBoxVerticalOffset, lineLogicalRect.left(), lineBox.logicalWidth(), lineBox.logicalHeight() };
        formattingState.addLine({ lineLogicalRect, lineBoxLogicalRect, lineBoxVerticalOffset + lineBox.alignmentBaseline(), lineBox.horizontalAlignmentOffset().valueOr(InlineLayoutUnit { }) });
    };
    constructLineGeometry();
    return lineLogicalRect;
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
