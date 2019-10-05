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

#include "InlineFormattingState.h"
#include "InlineTextItem.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutState.h"
#include "Logging.h"
#include "Textutil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Container& formattingContextRoot, InlineFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

static inline const Box* nextInPreOrder(const Box& layoutBox, const Container& stayWithin)
{
    const Box* nextInPreOrder = nullptr;
    if (!layoutBox.establishesFormattingContext() && is<Container>(layoutBox) && downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
        return downcast<Container>(layoutBox).firstInFlowOrFloatingChild();

    for (nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &stayWithin; nextInPreOrder = nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
}

void InlineFormattingContext::layoutInFlowContent()
{
    if (!root().hasInFlowOrFloatingChild())
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    auto& rootGeometry = geometryForBox(root());
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { rootGeometry } };
    auto usedVerticalValues = UsedVerticalValues { UsedVerticalValues::Constraints { rootGeometry } };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, paddings and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext())
            layoutFormattingContextRoot(*layoutBox, usedHorizontalValues, usedVerticalValues);
        else
            computeHorizontalAndVerticalGeometry(*layoutBox, usedHorizontalValues, usedVerticalValues);
        layoutBox = nextInPreOrder(*layoutBox, root());
    }

    // FIXME: This is such a waste when intrinsic width computation already collected the inline items.
    formattingState().inlineItems().clear();
    formattingState().inlineRuns().clear();

    collectInlineContent();
    lineLayout(usedHorizontalValues);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayout(UsedHorizontalValues usedHorizontalValues)
{
    auto& inlineItems = formattingState().inlineItems();
    auto lineLogicalTop = geometryForBox(root()).contentBoxTop();
    LineLayout::IndexAndRange currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        auto lineConstraints = initialConstraintsForLine(usedHorizontalValues, lineLogicalTop);
        auto lineInput = LineLayout::LineInput { lineConstraints, root().style().textAlign(), currentInlineItem, inlineItems };
        auto lineLayout = LineLayout { *this, lineInput };

        auto lineContent = lineLayout.layout();
        setDisplayBoxesForLine(lineContent, usedHorizontalValues);

        if (lineContent.lastCommitted) {
            currentInlineItem = { lineContent.lastCommitted->index + 1, WTF::nullopt };
            lineLogicalTop = lineContent.lineBox.logicalBottom();
        } else {
            // Floats prevented us placing any content on the line.
            ASSERT(lineInput.initialConstraints.lineIsConstrainedByFloat);
            // Move the next line below the intrusive float.
            auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
            auto floatConstraints = floatingContext.constraints({ lineLogicalTop });
            ASSERT(floatConstraints.left || floatConstraints.right);
            static auto inifitePoint = PointInContextRoot::max();
            // In case of left and right constraints, we need to pick the one that's closer to the current line.
            lineLogicalTop = std::min(floatConstraints.left.valueOr(inifitePoint).y, floatConstraints.right.valueOr(inifitePoint).y);
            ASSERT(lineLogicalTop < inifitePoint.y);
        }
    }
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& formattingContextRoot, UsedHorizontalValues usedHorizontalValues, UsedVerticalValues usedVerticalValues)
{
    ASSERT(formattingContextRoot.isFloatingPositioned() || formattingContextRoot.isInlineBlockBox());

    computeBorderAndPadding(formattingContextRoot, usedHorizontalValues);
    computeWidthAndMargin(formattingContextRoot, usedHorizontalValues);
    // Swich over to the new formatting context (the one that the root creates).
    if (is<Container>(formattingContextRoot)) {
        auto& rootContainer = downcast<Container>(formattingContextRoot);
        auto formattingContext = LayoutContext::createFormattingContext(rootContainer, layoutState());
        formattingContext->layoutInFlowContent();
        // Come back and finalize the root's height and margin.
        computeHeightAndMargin(rootContainer, usedHorizontalValues, usedVerticalValues);
        // Now that we computed the root's height, we can go back and layout the out-of-flow content.
        formattingContext->layoutOutOfFlowContent();
    } else
        computeHeightAndMargin(formattingContextRoot, usedHorizontalValues, usedVerticalValues);
}

void InlineFormattingContext::computeHorizontalAndVerticalGeometry(const Box& layoutBox, UsedHorizontalValues usedHorizontalValues, UsedVerticalValues usedVerticalValues)
{
    if (is<Container>(layoutBox)) {
        // Inline containers (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and paddings.
        computeHorizontalMargin(layoutBox, usedHorizontalValues);
        computeBorderAndPadding(layoutBox, usedHorizontalValues);
        // Inline containers have 0 computed vertical margins.
        formattingState().displayBox(layoutBox).setVerticalMargin({ { }, { } });
        return;
    }

    if (layoutBox.isReplaced()) {
        // Replaced elements (img, video) can be sized but not yet positioned.
        computeBorderAndPadding(layoutBox, usedHorizontalValues);
        computeWidthAndMargin(layoutBox, usedHorizontalValues);
        computeHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
        return;
    }

    // These are actual text boxes. No margins, borders or paddings.
    ASSERT(layoutBox.isAnonymous() || layoutBox.isLineBreakBox());
    auto& displayBox = formattingState().displayBox(layoutBox);

    displayBox.setVerticalMargin({ { }, { } });
    displayBox.setHorizontalMargin({ });
    displayBox.setBorder({ { }, { } });
    displayBox.setPadding({ });
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
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { { }, { } } };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext()) {
            formattingContextRootList.append(layoutBox);
            computeIntrinsicWidthForFormattingRoot(*layoutBox, usedHorizontalValues);
        } else if (layoutBox->isReplaced() || is<Container>(*layoutBox)) {
            computeBorderAndPadding(*layoutBox, usedHorizontalValues);
            // inline-block and replaced.
            auto needsWidthComputation = layoutBox->isReplaced();
            if (needsWidthComputation)
                computeWidthAndMargin(*layoutBox, usedHorizontalValues);
            else {
                // Simple inline container with no intrinsic width <span>.
                computeHorizontalMargin(*layoutBox, usedHorizontalValues);
            }
        }
        layoutBox = nextInPreOrder(*layoutBox, root());
    }

    collectInlineContent();

    auto maximumLineWidth = [&](auto availableWidth) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& displayBox = formattingState().displayBox(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - displayBox.horizontalMarginBorderAndPadding();
            displayBox.setContentBoxWidth(contentWidth);
        }
        auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { { }, availableWidth } };
        return computedIntrinsicWidthForConstraint(usedHorizontalValues);
    };

    auto constraints = geometry().constrainByMinMaxWidth(root(), { maximumLineWidth(0), maximumLineWidth(LayoutUnit::max()) });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

LayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(UsedHorizontalValues usedHorizontalValues) const
{
    auto& inlineItems = formattingState().inlineItems();
    LayoutUnit maximumLineWidth;
    LineLayout::IndexAndRange currentInlineItem;
    while (currentInlineItem.index < inlineItems.size()) {
        // Only the horiztonal available width is constrained when computing intrinsic width.
        auto initialLineConstraints = Line::InitialConstraints { { }, usedHorizontalValues.constraints.width, false, { } };
        auto lineInput = LineLayout::LineInput { initialLineConstraints, currentInlineItem, inlineItems };

        auto lineContent = LineLayout(*this, lineInput).layout();

        currentInlineItem = { lineContent.lastCommitted->index + 1, { } };
        LayoutUnit floatsWidth;
        for (auto& floatItem : lineContent.floats)
            floatsWidth += geometryForBox(floatItem->layoutBox()).marginBoxWidth();
        maximumLineWidth = std::max(maximumLineWidth, floatsWidth + lineContent.lineBox.logicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot, UsedHorizontalValues usedHorizontalValues)
{
    ASSERT(formattingRoot.establishesFormattingContext());

    computeBorderAndPadding(formattingRoot, usedHorizontalValues);
    computeHorizontalMargin(formattingRoot, usedHorizontalValues);

    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = geometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else if (is<Container>(formattingRoot))
        constraints = LayoutContext::createFormattingContext(downcast<Container>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    constraints = geometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, UsedHorizontalValues usedHorizontalValues)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, usedHorizontalValues);
    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setHorizontalComputedMargin(computedHorizontalMargin);
    displayBox.setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, UsedHorizontalValues usedHorizontalValues)
{
    WidthAndMargin widthAndMargin;
    if (layoutBox.isFloatingPositioned())
        widthAndMargin = geometry().floatingWidthAndMargin(layoutBox, usedHorizontalValues);
    else if (layoutBox.isInlineBlockBox())
        widthAndMargin = geometry().inlineBlockWidthAndMargin(layoutBox, usedHorizontalValues);
    else if (layoutBox.replaced())
        widthAndMargin = geometry().inlineReplacedWidthAndMargin(layoutBox, usedHorizontalValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(widthAndMargin.computedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, UsedHorizontalValues usedHorizontalValues, UsedVerticalValues usedVerticalValues)
{
    HeightAndMargin heightAndMargin;
    if (layoutBox.isFloatingPositioned())
        heightAndMargin = geometry().floatingHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else if (layoutBox.isInlineBlockBox())
        heightAndMargin = geometry().inlineBlockHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else if (layoutBox.replaced())
        heightAndMargin = geometry().inlineReplacedHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalMargin({ heightAndMargin.nonCollapsedMargin, { } });
}

void InlineFormattingContext::computeWidthAndHeightForReplacedInlineBox(const Box& layoutBox, UsedHorizontalValues usedHorizontalValues, UsedVerticalValues usedVerticalValues)
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());
    ASSERT(layoutBox.replaced());

    computeBorderAndPadding(layoutBox, usedHorizontalValues);
    computeWidthAndMargin(layoutBox, usedHorizontalValues);
    computeHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
}

void InlineFormattingContext::collectInlineContent()
{
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [ContainerStart][InlineBox][ContainerStart][ContainerEnd][InlineBox][ContainerEnd]
    auto& formattingState = this->formattingState();
    LayoutQueue layoutQueue;
    if (root().hasInFlowOrFloatingChild())
        layoutQueue.append(root().firstInFlowOrFloatingChild());
    while (!layoutQueue.isEmpty()) {
        auto treatAsInlineContainer = [](auto& layoutBox) {
            return is<Container>(layoutBox) && !layoutBox.establishesFormattingContext();
        };
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            if (!treatAsInlineContainer(layoutBox))
                break;
            // This is the start of an inline container (e.g. <span>).
            formattingState.addInlineItem(makeUnique<InlineItem>(layoutBox, InlineItem::Type::ContainerStart));
            auto& container = downcast<Container>(layoutBox);
            if (!container.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(container.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            // This is the end of an inline container (e.g. </span>).
            if (treatAsInlineContainer(layoutBox))
                formattingState.addInlineItem(makeUnique<InlineItem>(layoutBox, InlineItem::Type::ContainerEnd));
            else if (layoutBox.isLineBreakBox())
                formattingState.addInlineItem(makeUnique<InlineItem>(layoutBox, InlineItem::Type::HardLineBreak));
            else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem(makeUnique<InlineItem>(layoutBox, InlineItem::Type::Float));
            else {
                ASSERT(layoutBox.isInlineLevelBox());
                if (layoutBox.hasTextContent())
                    InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), layoutBox);
                else
                    formattingState.addInlineItem(makeUnique<InlineItem>(layoutBox, InlineItem::Type::Box));
            }

            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

Line::InitialConstraints InlineFormattingContext::initialConstraintsForLine(UsedHorizontalValues usedHorizontalValues, const LayoutUnit lineLogicalTop)
{
    auto lineLogicalLeft = geometryForBox(root()).contentBoxLeft();
    auto availableWidth = usedHorizontalValues.constraints.width;
    auto lineIsConstrainedByFloat = false;

    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!floatingContext.isEmpty()) {
        auto floatConstraints = floatingContext.constraints({ lineLogicalTop });
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && floatConstraints.left->x <= lineLogicalLeft)
            floatConstraints.left = { };

        auto lineLogicalRight = geometryForBox(root()).contentBoxRight();
        if (floatConstraints.right && floatConstraints.right->x >= lineLogicalRight)
            floatConstraints.right = { };

        lineIsConstrainedByFloat = floatConstraints.left || floatConstraints.right;

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(floatConstraints.left->x <= floatConstraints.right->x);
            availableWidth = floatConstraints.right->x - floatConstraints.left->x;
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.left) {
            ASSERT(floatConstraints.left->x >= lineLogicalLeft);
            availableWidth -= (floatConstraints.left->x - lineLogicalLeft);
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.right) {
            ASSERT(floatConstraints.right->x >= lineLogicalLeft);
            availableWidth = floatConstraints.right->x - lineLogicalLeft;
        }
    }
    return Line::InitialConstraints { { lineLogicalLeft, lineLogicalTop }, availableWidth, lineIsConstrainedByFloat, quirks().lineHeightConstraints(root()) };
}

void InlineFormattingContext::setDisplayBoxesForLine(const LineLayout::LineContent& lineContent, UsedHorizontalValues usedHorizontalValues)
{
    auto& formattingState = this->formattingState();

    if (!lineContent.floats.isEmpty()) {
        auto floatingContext = FloatingContext { root(), *this, formattingState.floatingState() };
        // Move floats to their final position.
        for (const auto& floatItem : lineContent.floats) {
            auto& floatBox = floatItem->layoutBox();
            auto& displayBox = formattingState.displayBox(floatBox);
            // Set static position first.
            auto& lineBox = lineContent.lineBox;
            displayBox.setTopLeft({ lineBox.logicalLeft(), lineBox.logicalTop() });
            // Float it.
            displayBox.setTopLeft(floatingContext.positionForFloat(floatBox));
            floatingContext.append(floatBox);
        }
    }

    // Add final display runs to state.
    for (auto& lineRun : lineContent.runList) {
        // Inline level containers (<span>) don't generate inline runs.
        if (lineRun->isContainerStart() || lineRun->isContainerEnd())
            continue;
        // Collapsed line runs don't generate display runs.
        if (lineRun->isVisuallyEmpty())
            continue;
        formattingState.addInlineRun(lineRun->displayRun());
    }

    // Compute box final geometry.
    auto& lineRuns = lineContent.runList;
    for (unsigned index = 0; index < lineRuns.size(); ++index) {
        auto& lineRun = lineRuns.at(index);
        auto& logicalRect = lineRun->logicalRect();
        auto& layoutBox = lineRun->layoutBox();
        auto& displayBox = formattingState.displayBox(layoutBox);

        if (lineRun->isLineBreak()) {
            displayBox.setTopLeft(logicalRect.topLeft());
            displayBox.setContentBoxWidth(logicalRect.width());
            displayBox.setContentBoxHeight(logicalRect.height());
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (lineRun->isBox()) {
            auto topLeft = logicalRect.topLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += geometry().inFlowPositionedPositionOffset(layoutBox, usedHorizontalValues);
            displayBox.setTopLeft(topLeft);
            continue;
        }

        // Inline level container start (<span>)
        if (lineRun->isContainerStart()) {
            displayBox.setTopLeft(logicalRect.topLeft());
            continue;
        }

        // Inline level container end (</span>)
        if (lineRun->isContainerEnd()) {
            if (layoutBox.isInFlowPositioned()) {
                auto inflowOffset = geometry().inFlowPositionedPositionOffset(layoutBox, usedHorizontalValues);
                displayBox.moveHorizontally(inflowOffset.width());
                displayBox.moveVertically(inflowOffset.height());
            }
            auto marginBoxWidth = logicalRect.left() - displayBox.left();
            auto contentBoxWidth = marginBoxWidth - (displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0));
            // FIXME fix it for multiline.
            displayBox.setContentBoxWidth(contentBoxWidth);
            displayBox.setContentBoxHeight(logicalRect.height());
            continue;
        }

        if (lineRun->isText()) {
            const Line::Run* previousLineRun = !index ? nullptr : lineRuns[index - 1].get();
            // FIXME take content breaking into account when part of the layout box is on the previous line.
            auto firstInlineRunForLayoutBox = !previousLineRun || &previousLineRun->layoutBox() != &layoutBox;
            auto logicalWidth = lineRun->isVisuallyEmpty() ? LayoutUnit() : logicalRect.width();
            if (firstInlineRunForLayoutBox) {
                // Setup display box for the associated layout box.
                displayBox.setTopLeft(logicalRect.topLeft());
                displayBox.setContentBoxWidth(logicalWidth);
                displayBox.setContentBoxHeight(logicalRect.height());
            } else {
                // FIXME fix it for multirun/multiline.
                displayBox.setContentBoxWidth(displayBox.contentBoxWidth() + logicalWidth);
            }
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    formattingState.addLineBox(lineContent.lineBox);
}

}
}

#endif
