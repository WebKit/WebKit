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

static inline const Box* nextInlineLevelBoxToLayout(const Box& layoutBox, const Container& stayWithin)
{
    // Atomic inline-level boxes and floats are opaque boxes meaning that they are
    // responsible for their own content (do not need to descend into their subtrees).
    // Only inline boxes may have relevant descendant content.
    if (layoutBox.isInlineBox()) {
        if (is<Container>(layoutBox) && downcast<Container>(layoutBox).hasInFlowOrFloatingChild()) {
            // Anonymous inline boxes/line breaks can't have descendant content by definition.
            ASSERT(!layoutBox.isAnonymous() && !layoutBox.isLineBreakBox());
            return downcast<Container>(layoutBox).firstInFlowOrFloatingChild();
        }
    }

    for (auto* nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &stayWithin; nextInPreOrder = nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
}

void InlineFormattingContext::layoutInFlowContent(InvalidationState& invalidationState, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints)
{
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    ASSERT(root().hasInFlowOrFloatingChild());

    invalidateFormattingState(invalidationState);
    auto* layoutBox = root().firstInFlowOrFloatingChild();
    // 1. Visit each inline box and partially compute their geometry (margins, paddings and borders).
    // 2. Collect the inline items (flatten the the layout tree) and place them on lines in bidirectional order. 
    while (layoutBox) {
        ASSERT(layoutBox->isInlineLevelBox());

        if (layoutBox->isAtomicInlineLevelBox()) {
            // Inline-blocks, inline-tables and replaced elements (img, video) can be sized but not yet positioned.
            computeBorderAndPadding(*layoutBox, horizontalConstraints);
            computeWidthAndMargin(*layoutBox, horizontalConstraints);
            auto createsFormattingContext = layoutBox->isInlineBlockBox() || layoutBox->isInlineTableBox();
            auto hasInFlowOrFloatingChild = is<Container>(*layoutBox) && downcast<Container>(*layoutBox).hasInFlowOrFloatingChild();
            if (createsFormattingContext && hasInFlowOrFloatingChild) {
                auto formattingContext = LayoutContext::createFormattingContext(downcast<Container>(*layoutBox), layoutState());
                formattingContext->layoutInFlowContent(invalidationState, horizontalConstraints, verticalConstraints);
            }
            computeHeightAndMargin(*layoutBox, horizontalConstraints);
            if (createsFormattingContext && is<Container>(*layoutBox) && downcast<Container>(*layoutBox).hasChild()) {
                auto& displayBox = geometryForBox(*layoutBox);
                auto horizontalConstraintsForOutOfFlow = Geometry::horizontalConstraintsForOutOfFlow(displayBox);
                auto verticalConstraintsForOutOfFlow = Geometry::verticalConstraintsForOutOfFlow(displayBox);
                auto formattingContext = LayoutContext::createFormattingContext(downcast<Container>(*layoutBox), layoutState());
                formattingContext->layoutOutOfFlowContent(invalidationState, horizontalConstraintsForOutOfFlow, verticalConstraintsForOutOfFlow);
            }
        } else if (layoutBox->isInlineBox()) {
            if (layoutBox->isAnonymous() || layoutBox->isLineBreakBox()) {
                // Text wrapper boxes are anonymous inline level boxes. Their computed border/padding/margins are 0.
                auto& displayBox = formattingState().displayBox(*layoutBox);
                displayBox.setVerticalMargin({ { }, { } });
                displayBox.setHorizontalMargin({ });
                displayBox.setBorder({ { }, { } });
                displayBox.setPadding({ });
            } else {
                // Inline boxes (<span>) can't get sized/positioned yet. At this point we can only compute their margins, borders and paddings.
                computeBorderAndPadding(*layoutBox, horizontalConstraints);
                computeHorizontalMargin(*layoutBox, horizontalConstraints);
                formattingState().displayBox(*layoutBox).setVerticalMargin({ { }, { } });
            }
        } else
            ASSERT_NOT_REACHED();

        layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
    }

    collectInlineContentIfNeeded();

    auto& inlineItems = formattingState().inlineItems();
    lineLayout(inlineItems, { 0, inlineItems.size() }, horizontalConstraints, verticalConstraints);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

void InlineFormattingContext::lineLayout(InlineItems& inlineItems, LineLayoutContext::InlineItemRange layoutRange, const HorizontalConstraints& horizontalConstraints, const VerticalConstraints& verticalConstraints)
{
    auto lineLogicalTop = verticalConstraints.logicalTop;
    Optional<unsigned> partialLeadingContentLength;
    auto lineBuilder = LineBuilder { *this, root().style().textAlign(), LineBuilder::IntrinsicSizing::No };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };

    while (!layoutRange.isEmpty()) {
        lineBuilder.initialize(constraintsForLine(horizontalConstraints, lineLogicalTop));
        auto lineContent = lineLayoutContext.layoutLine(lineBuilder, layoutRange, partialLeadingContentLength);
        setDisplayBoxesForLine(lineContent, horizontalConstraints);

        partialLeadingContentLength = { };
        if (lineContent.trailingInlineItemIndex) {
            lineLogicalTop = lineContent.lineBox.logicalBottom();
            // When the trailing content is partial, we need to reuse the last InlinItem.
            if (lineContent.partialContent) {
                layoutRange.start = *lineContent.trailingInlineItemIndex;
                // Turn previous line's overflow content length into the next line's leading content partial length.
                // "sp<->litcontent" -> overflow length: 10 -> leading partial content length: 10. 
                partialLeadingContentLength = lineContent.partialContent->overflowContentLength;
            } else
                layoutRange.start = *lineContent.trailingInlineItemIndex + 1;
            continue;
        }
        // Floats prevented us placing any content on the line.
        ASSERT(lineBuilder.hasIntrusiveFloat());
        // Move the next line below the intrusive float.
        auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
        auto floatConstraints = floatingContext.constraints(lineLogicalTop, toLayoutUnit(lineContent.lineBox.logicalBottom()) );
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
    // In order to compute the max/min widths, we need to compute margins, borders and paddings for certain inline boxes first.
    while (layoutBox) {
        if (layoutBox->isAnonymous()) {
            layoutBox = nextInlineLevelBoxToLayout(*layoutBox, root());
            continue;
        }
        if (layoutBox->isReplaced()) {
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
            auto& displayBox = formattingState().displayBox(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - displayBox.horizontalMarginBorderAndPadding();
            displayBox.setContentBoxWidth(contentWidth);
        }
        return computedIntrinsicWidthForConstraint({ 0_lu, toLayoutUnit(availableWidth) });
    };

    auto constraints = geometry().constrainByMinMaxWidth(root(), { toLayoutUnit(maximumLineWidth(0)), toLayoutUnit(maximumLineWidth(maxInlineLayoutUnit())) });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

InlineLayoutUnit InlineFormattingContext::computedIntrinsicWidthForConstraint(const HorizontalConstraints& horizontalConstraints) const
{
    auto& inlineItems = formattingState().inlineItems();
    auto maximumLineWidth = InlineLayoutUnit { };
    auto lineBuilder = LineBuilder { *this, root().style().textAlign(), LineBuilder::IntrinsicSizing::Yes };
    auto lineLayoutContext = LineLayoutContext { *this, root(), inlineItems };
    auto layoutRange = LineLayoutContext::InlineItemRange { 0 , inlineItems.size() };
    while (!layoutRange.isEmpty()) {
        // Only the horiztonal available width is constrained when computing intrinsic width.
        lineBuilder.initialize(LineBuilder::Constraints { { }, horizontalConstraints.logicalWidth, false, { } });
        auto lineContent = lineLayoutContext.layoutLine(lineBuilder, layoutRange , { });

        layoutRange.start = *lineContent.trailingInlineItemIndex + 1;
        InlineLayoutUnit floatsWidth = 0;
        for (auto& floatItem : lineContent.floats)
            floatsWidth += geometryForBox(floatItem->layoutBox()).marginBoxWidth();
        maximumLineWidth = std::max(maximumLineWidth, floatsWidth + lineContent.lineBox.logicalWidth());
    }
    return maximumLineWidth;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());
    auto constraints = IntrinsicWidthConstraints { };
    if (auto fixedWidth = geometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else if (is<Container>(formattingRoot) && downcast<Container>(formattingRoot).hasInFlowOrFloatingChild())
        constraints = LayoutContext::createFormattingContext(downcast<Container>(formattingRoot), layoutState())->computedIntrinsicWidthConstraints();
    constraints = geometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(geometryForBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, horizontalConstraints);
    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setHorizontalComputedMargin(computedHorizontalMargin);
    displayBox.setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    ContentWidthAndMargin contentWidthAndMargin;
    // FIXME: Add support for min/max-width.
    auto usedWidth = OverrideHorizontalValues { };
    if (layoutBox.isFloatingPositioned())
        contentWidthAndMargin = geometry().floatingWidthAndMargin(layoutBox, horizontalConstraints, usedWidth);
    else if (layoutBox.isInlineBlockBox())
        contentWidthAndMargin = geometry().inlineBlockWidthAndMargin(layoutBox, horizontalConstraints, usedWidth);
    else if (layoutBox.replaced())
        contentWidthAndMargin = geometry().inlineReplacedWidthAndMargin(layoutBox, horizontalConstraints, usedWidth);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxWidth(contentWidthAndMargin.contentWidth);
    displayBox.setHorizontalMargin(contentWidthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(contentWidthAndMargin.computedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox, const HorizontalConstraints& horizontalConstraints)
{
    ContentHeightAndMargin contentHeightAndMargin;
    // FIXME: Add min/max-height support.
    auto usedHeight = OverrideVerticalValues { };
    if (layoutBox.isFloatingPositioned())
        contentHeightAndMargin = geometry().floatingHeightAndMargin(layoutBox, horizontalConstraints, usedHeight);
    else if (layoutBox.isInlineBlockBox())
        contentHeightAndMargin = geometry().inlineBlockHeightAndMargin(layoutBox, horizontalConstraints, usedHeight);
    else if (layoutBox.replaced())
        contentHeightAndMargin = geometry().inlineReplacedHeightAndMargin(layoutBox, horizontalConstraints, { }, usedHeight);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = formattingState().displayBox(layoutBox);
    displayBox.setContentBoxHeight(contentHeightAndMargin.contentHeight);
    displayBox.setVerticalMargin({ contentHeightAndMargin.nonCollapsedMargin, { } });
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
            auto isBoxWithInlineContent = layoutBox.isInlineBox() && !layoutBox.isAnonymous() && !layoutBox.isLineBreakBox();
            if (!isBoxWithInlineContent)
                break;
            // This is the start of an inline box (e.g. <span>).
            formattingState.addInlineItem({ layoutBox, InlineItem::Type::ContainerStart });
            auto& inlineBoxWithInlineContent = downcast<Container>(layoutBox);
            if (!inlineBoxWithInlineContent.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(inlineBoxWithInlineContent.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (layoutBox.isLineBreakBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::HardLineBreak });
            else if (layoutBox.isFloatingPositioned())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Float });
            else if (layoutBox.isAtomicInlineLevelBox())
                formattingState.addInlineItem({ layoutBox, InlineItem::Type::Box });
            else if (layoutBox.isAnonymous()) {
                ASSERT(layoutBox.hasTextContent());
                InlineTextItem::createAndAppendTextItems(formattingState.inlineItems(), layoutBox);
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

LineBuilder::Constraints InlineFormattingContext::constraintsForLine(const HorizontalConstraints& horizontalConstraints, InlineLayoutUnit lineLogicalTop)
{
    auto lineLogicalLeft = horizontalConstraints.logicalLeft;
    auto lineLogicalRight = lineLogicalLeft + horizontalConstraints.logicalWidth;
    auto lineHeightAndBaseline = quirks().lineHeightConstraints(root());
    auto lineIsConstrainedByFloat = false;

    auto floatingContext = FloatingContext { root(), *this, formattingState().floatingState() };
    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    if (!floatingContext.isEmpty()) {
        // FIXME: Add support for variable line height, where the intrusive floats should be probed as the line height grows.
        auto floatConstraints = floatingContext.constraints(toLayoutUnit(lineLogicalTop), toLayoutUnit(lineLogicalTop + lineHeightAndBaseline.height));
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && floatConstraints.left->x <= lineLogicalLeft)
            floatConstraints.left = { };

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
            isFormattingContextRootCandidateToTextIndent = RuntimeEnabledFeatures::sharedFeatures().layoutFormattingContextIntegrationEnabled()
                ? layoutState().isIntegratedRootBoxFirstChild()
                : root.parent()->firstInFlowChild() == &root;
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
        return geometry().computedTextIndent(root, horizontalConstraints).valueOr(InlineLayoutUnit { });
    };
    lineLogicalLeft += computedTextIndent();
    return LineBuilder::Constraints { { lineLogicalLeft, lineLogicalTop }, lineLogicalRight - lineLogicalLeft, lineIsConstrainedByFloat, lineHeightAndBaseline };
}

void InlineFormattingContext::setDisplayBoxesForLine(const LineLayoutContext::LineContent& lineContent, const HorizontalConstraints& horizontalConstraints)
{
    auto& formattingState = this->formattingState();
    auto& lineBox = lineContent.lineBox;

    if (!lineContent.floats.isEmpty()) {
        auto floatingContext = FloatingContext { root(), *this, formattingState.floatingState() };
        // Move floats to their final position.
        for (const auto& floatItem : lineContent.floats) {
            auto& floatBox = floatItem->layoutBox();
            auto& displayBox = formattingState.displayBox(floatBox);
            // Set static position first.
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
        initialContaingBlockSize = geometryForBox(root().initialContainingBlock(), EscapeReason::StrokeOverflowNeedsViewportGeometry).contentBox().size();
    auto& inlineContent = formattingState.ensureDisplayInlineContent();
    auto lineIndex = inlineContent.lineBoxes.size();
    auto lineInkOverflow = lineBox.scrollableOverflow();
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
        auto initiatesInlineRun = !lineRun.isContainerStart() && !lineRun.isContainerEnd();
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
                topLeft += geometry().inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);
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
                auto inflowOffset = geometry().inFlowPositionedPositionOffset(layoutBox, horizontalConstraints);
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
    // FIXME: This is where the logical to physical translate should happen.
    auto& baseline = lineBox.baseline();
    inlineContent.lineBoxes.append({ lineBox.logicalRect(), lineBox.scrollableOverflow(), lineInkOverflow, { baseline.ascent(), baseline.descent() }, lineBox.baselineOffset(), lineBox.isConsideredEmpty() });
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
