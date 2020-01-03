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
#include "InvalidationState.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutState.h"
#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include "TextUtil.h"
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

void InlineFormattingContext::layoutInFlowContent(InvalidationState& invalidationState)
{
    if (!root().hasInFlowOrFloatingChild())
        return;

    invalidateFormattingState(invalidationState);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    auto& rootGeometry = geometryForBox(root());
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { rootGeometry } };
    auto usedVerticalValues = UsedVerticalValues { UsedVerticalValues::Constraints { rootGeometry } };
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, paddings and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext())
            layoutFormattingContextRoot(*layoutBox, invalidationState, usedHorizontalValues, usedVerticalValues);
        else
            computeHorizontalAndVerticalGeometry(*layoutBox, usedHorizontalValues, usedVerticalValues);
        layoutBox = nextInPreOrder(*layoutBox, root());
    }

    collectInlineContentIfNeeded();
    lineLayout(usedHorizontalValues);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayout(const UsedHorizontalValues& usedHorizontalValues)
{
    auto& inlineItems = formattingState().inlineItems();
    auto lineLogicalTop = geometryForBox(root()).contentBoxTop();
    unsigned leadingInlineItemIndex = 0;
    Optional<unsigned> partialLeadingContentLength;
    auto lineBuilder = LineBuilder { *this, root().style().textAlign(), LineBuilder::IntrinsicSizing::No };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };

    while (leadingInlineItemIndex < inlineItems.size()) {
        lineBuilder.initialize(constraintsForLine(usedHorizontalValues, lineLogicalTop));
        auto lineContent = lineLayoutContext.layoutLine(lineBuilder, leadingInlineItemIndex, partialLeadingContentLength);
        setDisplayBoxesForLine(lineContent, usedHorizontalValues);

        partialLeadingContentLength = { };
        if (lineContent.trailingInlineItemIndex) {
            lineLogicalTop = lineContent.lineBox.logicalBottom();
            // When the trailing content is partial, we need to reuse the last InlinItem.
            if (lineContent.partialContent) {
                leadingInlineItemIndex = *lineContent.trailingInlineItemIndex;
                // Turn previous line's overflow content length into the next line's leading content partial length.
                // "sp<->litcontent" -> overflow length: 10 -> leading partial content length: 10. 
                partialLeadingContentLength = lineContent.partialContent->overflowContentLength;
            } else
                leadingInlineItemIndex = *lineContent.trailingInlineItemIndex + 1;
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(lineBuilder.hasIntrusiveFloat());
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

void InlineFormattingContext::layoutFormattingContextRoot(const Box& formattingContextRoot, InvalidationState& invalidationState, const UsedHorizontalValues& usedHorizontalValues, const UsedVerticalValues& usedVerticalValues)
{
    ASSERT(formattingContextRoot.isFloatingPositioned() || formattingContextRoot.isInlineBlockBox());

    computeBorderAndPadding(formattingContextRoot, usedHorizontalValues);
    computeWidthAndMargin(formattingContextRoot, usedHorizontalValues);
    // Swich over to the new formatting context (the one that the root creates).
    if (is<Container>(formattingContextRoot)) {
        auto& rootContainer = downcast<Container>(formattingContextRoot);
        auto formattingContext = LayoutContext::createFormattingContext(rootContainer, layoutState());
        formattingContext->layoutInFlowContent(invalidationState);
        // Come back and finalize the root's height and margin.
        computeHeightAndMargin(rootContainer, usedHorizontalValues, usedVerticalValues);
        // Now that we computed the root's height, we can go back and layout the out-of-flow content.
        formattingContext->layoutOutOfFlowContent(invalidationState);
    } else
        computeHeightAndMargin(formattingContextRoot, usedHorizontalValues, usedVerticalValues);
}

void InlineFormattingContext::computeHorizontalAndVerticalGeometry(const Box& layoutBox, const UsedHorizontalValues& usedHorizontalValues, const UsedVerticalValues& usedVerticalValues)
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
    auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { 0_lu, 0_lu } };
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

    collectInlineContentIfNeeded();

    auto maximumLineWidth = [&](auto availableWidth) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& displayBox = formattingState().displayBox(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - displayBox.horizontalMarginBorderAndPadding();
            displayBox.setContentBoxWidth(contentWidth);
        }
        auto usedHorizontalValues = UsedHorizontalValues { UsedHorizontalValues::Constraints { 0_lu, toLayoutUnit(availableWidth) } };
        return computedIntrinsicWidthForConstraint(usedHorizontalValues);
    };

    auto constraints = geometry().constrainByMinMaxWidth(root(), { toLayoutUnit(maximumLineWidth(0)), toLayoutUnit(maximumLineWidth(maxInlineLayoutUnit())) });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(const UsedHorizontalValues& usedHorizontalValues) const
{
    auto& inlineItems = formattingState().inlineItems();
    InlineLayoutUnit maximumLineWidth = 0;
    unsigned leadingInlineItemIndex = 0;
    auto lineBuilder = LineBuilder { *this, root().style().textAlign(), LineBuilder::IntrinsicSizing::Yes };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };
    while (leadingInlineItemIndex < inlineItems.size()) {
        // Only the horiztonal available width is constrained when computing intrinsic width.
        lineBuilder.initialize(LineBuilder::Constraints { { }, usedHorizontalValues.constraints.width, false, { } });
        auto lineContent = lineLayoutContext.layoutLine(lineBuilder, leadingInlineItemIndex, { });

        leadingInlineItemIndex = *lineContent.trailingInlineItemIndex + 1;
        InlineLayoutUnit floatsWidth = 0;
        for (auto& floatItem : lineContent.floats)
            floatsWidth += geometryForBox(floatItem->layoutBox()).marginBoxWidth();
        maximumLineWidth = std::max(maximumLineWidth, floatsWidth + lineContent.lineBox.logicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot, const UsedHorizontalValues& usedHorizontalValues)
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

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const UsedHorizontalValues& usedHorizontalValues)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, usedHorizontalValues);
    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setHorizontalComputedMargin(computedHorizontalMargin);
    displayBox.setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const UsedHorizontalValues& usedHorizontalValues)
{
    ContentWidthAndMargin contentWidthAndMargin;
    if (layoutBox.isFloatingPositioned())
        contentWidthAndMargin = geometry().floatingWidthAndMargin(layoutBox, usedHorizontalValues);
    else if (layoutBox.isInlineBlockBox())
        contentWidthAndMargin = geometry().inlineBlockWidthAndMargin(layoutBox, usedHorizontalValues);
    else if (layoutBox.replaced())
        contentWidthAndMargin = geometry().inlineReplacedWidthAndMargin(layoutBox, usedHorizontalValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin(contentWidthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(contentWidthAndMargin.computedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const UsedHorizontalValues& usedHorizontalValues, const UsedVerticalValues& usedVerticalValues)
{
    ContentHeightAndMargin contentHeightAndMargin;
    if (layoutBox.isFloatingPositioned())
        contentHeightAndMargin = geometry().floatingHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else if (layoutBox.isInlineBlockBox())
        contentHeightAndMargin = geometry().inlineBlockHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else if (layoutBox.replaced())
        contentHeightAndMargin = geometry().inlineReplacedHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    displayBox.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin, { } });
}

void InlineFormattingContext::computeWidthAndHeightForReplacedInlineBox(const Box& layoutBox, const UsedHorizontalValues& usedHorizontalValues, const UsedVerticalValues& usedVerticalValues)
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());
    ASSERT(layoutBox.replaced());

    computeBorderAndPadding(layoutBox, usedHorizontalValues);
    computeWidthAndMargin(layoutBox, usedHorizontalValues);
    computeHeightAndMargin(layoutBox, usedHorizontalValues, usedVerticalValues);
}

void InlineFormattingContext::collectInlineContentIfNeeded()
{
    auto& formattingState = this->formattingState();
    if (!formattingState.inlineItems().isEmpty())
        return;
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [ContainerStart][InlineBox][ContainerStart][ContainerEnd][InlineBox][ContainerEnd]
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

LineBuilder::Constraints InlineFormattingContext::constraintsForLine(const UsedHorizontalValues& usedHorizontalValues, InlineLayoutUnit lineLogicalTop)
{
    auto lineLogicalLeft = geometryForBox(root()).contentBoxLeft();
    auto lineLogicalRight = lineLogicalLeft + usedHorizontalValues.constraints.width;
    auto lineIsConstrainedByFloat = false;

    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!floatingContext.isEmpty()) {
        auto floatConstraints = floatingContext.constraints({ toLayoutUnit(lineLogicalTop) });
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && floatConstraints.left->x <= lineLogicalLeft)
            floatConstraints.left = { };

        auto lineLogicalRight = geometryForBox(root()).contentBoxRight();
        if (floatConstraints.right && floatConstraints.right->x >= lineLogicalRight)
            floatConstraints.right = { };

        lineIsConstrainedByFloat = floatConstraints.left || floatConstraints.right;

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(floatConstraints.left->x <= floatConstraints.right->x);
            lineLogicalRight = floatConstraints.right->x;
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.left) {
            ASSERT(floatConstraints.left->x >= lineLogicalLeft);
            lineLogicalLeft = floatConstraints.left->x;
        } else if (floatConstraints.right) {
            ASSERT(floatConstraints.right->x >= lineLogicalLeft);
            lineLogicalRight = floatConstraints.right->x;
        }
    }

    auto computedTextIndent = [&] {
        // text-indent property specifies the indentation applied to lines of inline content in a block.
        // The indent is treated as a margin applied to the start edge of the line box.
        // Unless otherwise specified, only lines that are the first formatted line of an element are affected.
        // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
        // FIXME: Add support for each-line.
        // [Integration] root()->parent() would normally produce a valid layout box.
        auto& root = this->root();
        auto isFormattingContextRootCandidateToTextIndent = !root.isAnonymous();
        if (root.isAnonymous()) {
            // Unless otherwise specified by the each-line and/or hanging keywords, only lines that are the first formatted line
            // of an element are affected.
            // For example, the first line of an anonymous block box is only affected if it is the first child of its parent element.
            isFormattingContextRootCandidateToTextIndent = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled() ?
            layoutState().isIntegratedRootBoxFirstChild() : root.parent()->firstInFlowChild() == &root;
        }
        if (!isFormattingContextRootCandidateToTextIndent)
            return InlineLayoutUnit { };
        auto invertLineRange = false;
#if ENABLE(CSS3_TEXT)
        invertLineRange = root.style().textIndentType() == TextIndentType::Hanging;
#endif
        auto isFirstLine = formattingState().ensureDisplayInlineContent().lineBoxes.isEmpty();
        // text-indent: hanging inverts which lines are affected.
        // inverted line range -> all the lines except the first one.
        // !inverted line range -> first line gets the indent.
        auto shouldIndent = invertLineRange != isFirstLine;
        if (!shouldIndent)
            return InlineLayoutUnit { };
        return geometry().computedTextIndent(root, usedHorizontalValues.constraints).valueOr(InlineLayoutUnit { });
    };
    lineLogicalLeft += computedTextIndent();
    return LineBuilder::Constraints { { lineLogicalLeft, lineLogicalTop }, lineLogicalRight - lineLogicalLeft, lineIsConstrainedByFloat, quirks().lineHeightConstraints(root()) };
}

void InlineFormattingContext::setDisplayBoxesForLine(const LineLayoutContext::LineContent& lineContent, const UsedHorizontalValues& usedHorizontalValues)
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

    auto initialContaingBlockSize = LayoutSize { };
    if (RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()) {
        // ICB is not the real ICB when lyoutFormattingContextIntegrationEnabled is on.
        initialContaingBlockSize = layoutState().viewportSize();
    } else
        initialContaingBlockSize = geometryForBox(root().initialContainingBlock()).contentBox().size();
    auto& inlineContent = formattingState.ensureDisplayInlineContent();
    auto lineIndex = inlineContent.lineBoxes.size();
    inlineContent.lineBoxes.append(lineContent.lineBox);
    auto lineInkOverflow = lineContent.lineBox.scrollableOverflow();
    Optional<unsigned> lastTextItemIndex;
    // Compute box final geometry.
    auto& lineRuns = lineContent.runList;
    for (unsigned index = 0; index < lineRuns.size(); ++index) {
        auto& lineRun = lineRuns.at(index);
        auto& logicalRect = lineRun.logicalRect();
        auto& layoutBox = lineRun.layoutBox();
        auto& displayBox = formattingState.displayBox(layoutBox);

        // Add final display runs to state first.
        // Inline level containers (<span>) don't generate display runs and neither do completely collapsed runs.
        auto initiatesInlineRun = !lineRun.isContainerStart() && !lineRun.isContainerEnd() && !lineRun.isCollapsedToVisuallyEmpty();
        if (initiatesInlineRun) {
            auto computedInkOverflow = [&] {
                // FIXME: Add support for non-text ink overflow.
                if (!lineRun.isText())
                    return logicalRect;
                auto& style = lineRun.style();
                auto inkOverflow = logicalRect;
                auto strokeOverflow = std::ceil(style.computedStrokeWidth(ceiledIntSize(initialContaingBlockSize)));
                inkOverflow.inflate(strokeOverflow);

                auto letterSpacing = style.fontCascade().letterSpacing();
                if (letterSpacing < 0) {
                    // Last letter's negative spacing shrinks logical rect. Push it to ink overflow.
                    inkOverflow.expandHorizontally(-letterSpacing);
                }
                return inkOverflow;
            };
            auto inkOverflow = computedInkOverflow();
            lineInkOverflow.expandToContain(inkOverflow);
            inlineContent.runs.append({ lineIndex, lineRun.layoutBox(), logicalRect, inkOverflow, lineRun.textContext() });
        }

        if (lineRun.isLineBreak()) {
            displayBox.setTopLeft(toLayoutPoint(logicalRect.topLeft()));
            displayBox.setContentBoxWidth(toLayoutUnit(logicalRect.width()));
            displayBox.setContentBoxHeight(toLayoutUnit(logicalRect.height()));
            continue;
        }

        // Inline level box (replaced or inline-block)
        if (lineRun.isBox()) {
            auto topLeft = logicalRect.topLeft();
            if (layoutBox.isInFlowPositioned())
                topLeft += geometry().inFlowPositionedPositionOffset(layoutBox, usedHorizontalValues);
            displayBox.setTopLeft(toLayoutPoint(topLeft));
            continue;
        }

        // Inline level container start (<span>)
        if (lineRun.isContainerStart()) {
            displayBox.setTopLeft(toLayoutPoint(logicalRect.topLeft()));
            continue;
        }

        // Inline level container end (</span>)
        if (lineRun.isContainerEnd()) {
            if (layoutBox.isInFlowPositioned()) {
                auto inflowOffset = geometry().inFlowPositionedPositionOffset(layoutBox, usedHorizontalValues);
                displayBox.moveHorizontally(inflowOffset.width());
                displayBox.moveVertically(inflowOffset.height());
            }
            auto marginBoxWidth = logicalRect.left() - displayBox.left();
            auto contentBoxWidth = marginBoxWidth - (displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0));
            // FIXME fix it for multiline.
            displayBox.setContentBoxWidth(toLayoutUnit(contentBoxWidth));
            displayBox.setContentBoxHeight(toLayoutUnit(logicalRect.height()));
            continue;
        }

        if (lineRun.isText()) {
            lastTextItemIndex = inlineContent.runs.size() - 1;
            auto firstRunForLayoutBox = !index || &lineRuns[index - 1].layoutBox() != &layoutBox; 
            if (firstRunForLayoutBox) {
                // Setup display box for the associated layout box.
                displayBox.setTopLeft(toLayoutPoint(logicalRect.topLeft()));
                displayBox.setContentBoxWidth(toLayoutUnit(logicalRect.width()));
                displayBox.setContentBoxHeight(toLayoutUnit(logicalRect.height()));
            } else {
                // FIXME fix it for multirun/multiline.
                displayBox.setContentBoxWidth(toLayoutUnit(displayBox.contentBoxWidth() + logicalRect.width()));
            }
            continue;
        }
        ASSERT_NOT_REACHED();
    }
    // Make sure the trailing text run gets a hyphen when it needs one.
    if (lineContent.partialContent && lineContent.partialContent->trailingContentNeedsHyphen)
        inlineContent.runs[*lastTextItemIndex].textContext()->setNeedsHyphen();
    inlineContent.lineBoxes.last().setInkOverflow(lineInkOverflow);
}

void InlineFormattingContext::invalidateFormattingState(const InvalidationState&)
{
    // Find out what we need to invalidate. This is where we add some smarts to do partial line layout.
    // For now let's just clear the runs.
    formattingState().clearDisplayInlineContent();
    // FIXME: This is also where we would delete inline items if their content changed.
}

}
}

#endif
