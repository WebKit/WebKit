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
#include "InlineLineBreaker.h"
#include "InlineRunProvider.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "LayoutState.h"
#include "Logging.h"
#include "Textutil.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Box& formattingContextRoot, InlineFormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

static inline const Box* nextInPreOrder(const Box& layoutBox, const Container& root)
{
    const Box* nextInPreOrder = nullptr;
    if (!layoutBox.establishesFormattingContext() && is<Container>(layoutBox) && downcast<Container>(layoutBox).hasInFlowOrFloatingChild())
        return downcast<Container>(layoutBox).firstInFlowOrFloatingChild();

    for (nextInPreOrder = &layoutBox; nextInPreOrder && nextInPreOrder != &root; nextInPreOrder = nextInPreOrder->parent()) {
        if (auto* nextSibling = nextInPreOrder->nextInFlowOrFloatingSibling())
            return nextSibling;
    }
    return nullptr;
}

void InlineFormattingContext::layout() const
{
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");
    auto& root = downcast<Container>(this->root());
    auto usedValues = UsedHorizontalValues { layoutState().displayBoxForLayoutBox(root).contentBoxWidth(), { }, { } };
    auto* layoutBox = root.firstInFlowOrFloatingChild();
    // Compute width/height for non-text content and margin/border/padding for inline containers.
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext())
            layoutFormattingContextRoot(*layoutBox, usedValues);
        else if (is<Container>(*layoutBox)) {
            auto& inlineContainer = downcast<InlineContainer>(*layoutBox);
            computeMargin(inlineContainer, usedValues);
            computeBorderAndPadding(inlineContainer, usedValues);
        } else if (layoutBox->isReplaced())
            computeWidthAndHeightForReplacedInlineBox(*layoutBox, usedValues);
        layoutBox = nextInPreOrder(*layoutBox, root);
    }

    InlineRunProvider inlineRunProvider;
    collectInlineContent(inlineRunProvider);
    LineLayout(*this).layout(inlineRunProvider);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root << ")");
}

FormattingContext::IntrinsicWidthConstraints InlineFormattingContext::intrinsicWidthConstraints() const
{
    ASSERT(is<Container>(root()));

    auto& layoutState = this->layoutState();
    auto& root = downcast<Container>(this->root());
    if (auto intrinsicWidthConstraints = layoutState.formattingStateForBox(root).intrinsicWidthConstraints(root))
        return *intrinsicWidthConstraints;

    Vector<const Box*> formattingContextRootList;
    auto usedValues = UsedHorizontalValues { };
    auto* layoutBox = root.firstInFlowOrFloatingChild();
    while (layoutBox) {
        if (layoutBox->isFloatingPositioned())
            ASSERT_NOT_IMPLEMENTED_YET();
        else if (layoutBox->isInlineBlockBox()) {
            computeIntrinsicWidthForFormattingContextRoot(*layoutBox);
            formattingContextRootList.append(layoutBox);
        } else if (layoutBox->isReplaced() || is<Container>(*layoutBox)) {
            computeBorderAndPadding(*layoutBox, usedValues);
            // inline-block and replaced.
            auto needsWidthComputation = layoutBox->isReplaced() || layoutBox->establishesFormattingContext();
            if (needsWidthComputation)
                computeWidthAndMargin(*layoutBox, usedValues);
            else {
                // Simple inline container with no intrinsic width <span>.
                computeMargin(downcast<InlineContainer>(*layoutBox), usedValues);
            }
        }
        layoutBox = nextInPreOrder(*layoutBox, root);
    }

    InlineRunProvider inlineRunProvider;
    collectInlineContent(inlineRunProvider);

    auto maximumLineWidth = [&](auto availableWidth) {
        LayoutUnit maxContentLogicalRight;
        auto lineBreaker = InlineLineBreaker { layoutState, formattingState().inlineContent(), inlineRunProvider.runs() };
        LayoutUnit lineLogicalRight;

        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraints(*formattingRoot);
            layoutState.displayBoxForLayoutBox(*formattingRoot).setContentBoxWidth(availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum);
        }

        while (auto run = lineBreaker.nextRun(lineLogicalRight, availableWidth, !lineLogicalRight)) {
            if (run->position == InlineLineBreaker::Run::Position::LineBegin)
                lineLogicalRight = 0;
            lineLogicalRight += run->width;

            maxContentLogicalRight = std::max(maxContentLogicalRight, lineLogicalRight);
        }
        return maxContentLogicalRight;
    };

    auto intrinsicWidthConstraints = FormattingContext::IntrinsicWidthConstraints { maximumLineWidth(0), maximumLineWidth(LayoutUnit::max()) };
    layoutState.formattingStateForBox(root).setIntrinsicWidthConstraints(root, intrinsicWidthConstraints);
    return intrinsicWidthConstraints;
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingContextRoot(const Box& layoutBox) const
{
    ASSERT(layoutBox.establishesFormattingContext());

    auto usedValues = UsedHorizontalValues { };
    computeBorderAndPadding(layoutBox, usedValues);
    computeMargin(downcast<InlineContainer>(layoutBox), usedValues);
    layoutState().createFormattingContext(layoutBox)->intrinsicWidthConstraints();
}

void InlineFormattingContext::computeMargin(const InlineContainer& inlineContainer, UsedHorizontalValues usedValues) const
{
    // Non-replaced and formatting root containers (<span></span>) don't have width property -> non width computation.
    ASSERT(!inlineContainer.replaced());

    auto& displayBox = layoutState().displayBoxForLayoutBox(inlineContainer);
    auto computedHorizontalMargin = Geometry::computedHorizontalMargin(inlineContainer, usedValues);
    displayBox.setHorizontalComputedMargin(computedHorizontalMargin);
    displayBox.setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeBorderAndPadding(const Box& layoutBox, UsedHorizontalValues usedValues) const
{
    auto& displayBox = layoutState().displayBoxForLayoutBox(layoutBox);
    displayBox.setBorder(Geometry::computedBorder(layoutBox));
    displayBox.setPadding(Geometry::computedPadding(layoutBox, usedValues));
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, UsedHorizontalValues usedValues) const
{
    auto& layoutState = this->layoutState();
    WidthAndMargin widthAndMargin;
    if (layoutBox.isFloatingPositioned())
        widthAndMargin = Geometry::floatingWidthAndMargin(layoutState, layoutBox, usedValues);
    else if (layoutBox.isInlineBlockBox())
        widthAndMargin = Geometry::inlineBlockWidthAndMargin(layoutState, layoutBox, usedValues);
    else if (layoutBox.replaced())
        widthAndMargin = Geometry::inlineReplacedWidthAndMargin(layoutState, layoutBox, usedValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(widthAndMargin.computedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    HeightAndMargin heightAndMargin;
    if (layoutBox.isFloatingPositioned())
        heightAndMargin = Geometry::floatingHeightAndMargin(layoutState, layoutBox, { });
    else if (layoutBox.isInlineBlockBox())
        heightAndMargin = Geometry::inlineBlockHeightAndMargin(layoutState, layoutBox);
    else if (layoutBox.replaced())
        heightAndMargin = Geometry::inlineReplacedHeightAndMargin(layoutState, layoutBox, { });
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalMargin({ heightAndMargin.nonCollapsedMargin, { } });
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& root, UsedHorizontalValues usedValues) const
{
    ASSERT(root.isFloatingPositioned() || root.isInlineBlockBox());
    ASSERT(usedValues.containingBlockWidth);

    computeBorderAndPadding(root, usedValues);
    computeWidthAndMargin(root, usedValues);
    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutState().createFormattingContext(root);
    formattingContext->layout();
    // Come back and finalize the root's height and margin.
    computeHeightAndMargin(root);
    // Now that we computed the root's height, we can go back and layout the out-of-flow descedants (if any).
    formattingContext->layoutOutOfFlowDescendants(root);
}

void InlineFormattingContext::computeWidthAndHeightForReplacedInlineBox(const Box& layoutBox, UsedHorizontalValues usedValues) const
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());
    ASSERT(layoutBox.replaced());
    ASSERT(usedValues.containingBlockWidth);

    computeBorderAndPadding(layoutBox, usedValues);
    computeWidthAndMargin(layoutBox, usedValues);
    computeHeightAndMargin(layoutBox);
}

static void addDetachingRules(InlineItem& inlineItem, Optional<LayoutUnit> nonBreakableStartWidth, Optional<LayoutUnit> nonBreakableEndWidth)
{
    OptionSet<InlineItem::DetachingRule> detachingRules;
    if (nonBreakableStartWidth) {
        detachingRules.add(InlineItem::DetachingRule::BreakAtStart);
        inlineItem.addNonBreakableStart(*nonBreakableStartWidth);
    }
    if (nonBreakableEndWidth) {
        detachingRules.add(InlineItem::DetachingRule::BreakAtEnd);
        inlineItem.addNonBreakableEnd(*nonBreakableEndWidth);
    }
    inlineItem.addDetachingRule(detachingRules);
}

static InlineItem& createAndAppendInlineItem(InlineRunProvider& inlineRunProvider, InlineContent& inlineContent, const Box& layoutBox)
{
    ASSERT(layoutBox.isInlineLevelBox() || layoutBox.isFloatingPositioned());
    auto inlineItem = std::make_unique<InlineItem>(layoutBox);
    auto* inlineItemPtr = inlineItem.get();
    inlineContent.add(WTFMove(inlineItem));
    inlineRunProvider.append(*inlineItemPtr);
    return *inlineItemPtr;
}

void InlineFormattingContext::collectInlineContent(InlineRunProvider& inlineRunProvider) const
{
    if (!is<Container>(root()))
        return;
    auto& root = downcast<Container>(this->root());
    if (!root.hasInFlowOrFloatingChild())
        return;
    // The logic here is very similar to BFC layout.
    // 1. Travers down the layout tree and collect "start" unbreakable widths (margin-left, border-left, padding-left)
    // 2. Create InlineItem per leaf inline box (text nodes, inline-blocks, floats) and set "start" unbreakable width on them. 
    // 3. Climb back and collect "end" unbreakable width and set it on the last InlineItem.
    auto& layoutState = this->layoutState();
    auto& inlineContent = formattingState().inlineContent();

    enum class NonBreakableWidthType { Start, End };
    auto nonBreakableWidth = [&](auto& container, auto type) {
        auto& displayBox = layoutState.displayBoxForLayoutBox(container);
        if (type == NonBreakableWidthType::Start)
            return displayBox.marginStart() + displayBox.borderLeft() + displayBox.paddingLeft().valueOr(0);
        return displayBox.marginEnd() + displayBox.borderRight() + displayBox.paddingRight().valueOr(0);
    };

    LayoutQueue layoutQueue;
    layoutQueue.append(root.firstInFlowOrFloatingChild());

    Optional<LayoutUnit> nonBreakableStartWidth;
    Optional<LayoutUnit> nonBreakableEndWidth;
    InlineItem* lastInlineItem = nullptr;
    while (!layoutQueue.isEmpty()) {
        while (true) {
            auto& layoutBox = *layoutQueue.last();
            if (!is<Container>(layoutBox))
                break;
            auto& container = downcast<Container>(layoutBox);

            if (container.establishesFormattingContext()) {
                // Formatting contexts are treated as leaf nodes.
                auto& inlineItem = createAndAppendInlineItem(inlineRunProvider, inlineContent, container);
                auto& displayBox = layoutState.displayBoxForLayoutBox(container);
                auto currentNonBreakableStartWidth = nonBreakableStartWidth.valueOr(0) + displayBox.marginStart() + nonBreakableEndWidth.valueOr(0);
                addDetachingRules(inlineItem, currentNonBreakableStartWidth, displayBox.marginEnd());
                nonBreakableStartWidth = { };
                nonBreakableEndWidth = { };

                // Formatting context roots take care of their subtrees. Continue with next sibling if exists.
                layoutQueue.removeLast();
                if (!container.nextInFlowOrFloatingSibling())
                    break;
                layoutQueue.append(container.nextInFlowOrFloatingSibling());
                continue;
            }

            // Check if this non-formatting context container has any non-breakable start properties (margin-left, border-left, padding-left)
            // <span style="padding-left: 5px"><span style="padding-left: 5px">foobar</span></span> -> 5px + 5px
            auto currentNonBreakableStartWidth = nonBreakableWidth(layoutBox, NonBreakableWidthType::Start);
            if (currentNonBreakableStartWidth || layoutBox.isPositioned())
                nonBreakableStartWidth = nonBreakableStartWidth.valueOr(0) + currentNonBreakableStartWidth;

            if (!container.hasInFlowOrFloatingChild())
                break;
            layoutQueue.append(container.firstInFlowOrFloatingChild());
        }

        while (!layoutQueue.isEmpty()) {
            auto& layoutBox = *layoutQueue.takeLast();
            if (is<Container>(layoutBox)) {
                // This is the end of an inline container. Compute the non-breakable end width and add it to the last inline box.
                // <span style="padding-right: 5px">foobar</span> -> 5px; last inline item -> "foobar"
                auto currentNonBreakableEndWidth = nonBreakableWidth(layoutBox, NonBreakableWidthType::End);
                if (currentNonBreakableEndWidth || layoutBox.isPositioned())
                    nonBreakableEndWidth = nonBreakableEndWidth.valueOr(0) + currentNonBreakableEndWidth;
                // Add it to the last inline box
                if (lastInlineItem) {
                    addDetachingRules(*lastInlineItem, { }, nonBreakableEndWidth);
                    nonBreakableEndWidth = { };
                }
            } else {
                // Leaf inline box
                auto& inlineItem = createAndAppendInlineItem(inlineRunProvider, inlineContent, layoutBox);
                // Add start and the (through empty containers) accumulated end width.
                // <span style="padding-left: 1px">foobar</span> -> nonBreakableStartWidth: 1px;
                // <span style="padding: 5px"></span>foobar -> nonBreakableStartWidth: 5px; nonBreakableEndWidth: 5px
                if (nonBreakableStartWidth || nonBreakableEndWidth) {
                    addDetachingRules(inlineItem, nonBreakableStartWidth.valueOr(0) + nonBreakableEndWidth.valueOr(0), { });
                    nonBreakableStartWidth = { };
                    nonBreakableEndWidth = { };
                }
                lastInlineItem = &inlineItem;
            }

            if (auto* nextSibling = layoutBox.nextInFlowOrFloatingSibling()) {
                layoutQueue.append(nextSibling);
                break;
            }
        }
    }
}

}
}

#endif
