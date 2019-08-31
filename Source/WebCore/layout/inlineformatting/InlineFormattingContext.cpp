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
#include "InlineTextItem.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
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

void InlineFormattingContext::layout()
{
    if (!is<Container>(root()))
        return;

    auto& root = downcast<Container>(this->root());
    if (!root.hasInFlowOrFloatingChild())
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root << ")");
    auto availableWidth = displayBoxForLayoutBox(root).contentBoxWidth();
    auto usedValues = UsedHorizontalValues { availableWidth };
    auto* layoutBox = root.firstInFlowOrFloatingChild();
    // Compute width/height for non-text content and margin/border/padding for inline containers.
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext())
            layoutFormattingContextRoot(*layoutBox, usedValues);
        else if (is<Container>(*layoutBox))
            computeMarginBorderAndPaddingForInlineContainer(downcast<Container>(*layoutBox), usedValues);
        else if (layoutBox->isReplaced())
            computeWidthAndHeightForReplacedInlineBox(*layoutBox, usedValues);
        else {
            ASSERT(layoutBox->isInlineLevelBox());
            initializeMarginBorderAndPaddingForGenericInlineBox(*layoutBox);
        }
        layoutBox = nextInPreOrder(*layoutBox, root);
    }

    // FIXME: This is such a waste when intrinsic width computation already collected the inline items.
    formattingState().inlineItems().clear();
    formattingState().inlineRuns().clear();

    collectInlineContent();
    InlineLayout(*this).layout(formattingState().inlineItems(), availableWidth);
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root << ")");
}

FormattingContext::IntrinsicWidthConstraints InlineFormattingContext::computedIntrinsicWidthConstraints()
{
    auto& layoutState = this->layoutState();
    ASSERT(!formattingState().intrinsicWidthConstraints());

    if (!is<Container>(root()) || !downcast<Container>(root()).hasInFlowOrFloatingChild()) {
        auto constraints = geometry().constrainByMinMaxWidth(root(), { });
        formattingState().setIntrinsicWidthConstraints(constraints);
        return constraints;
    }

    auto& root = downcast<Container>(this->root());
    Vector<const Box*> formattingContextRootList;
    auto usedValues = UsedHorizontalValues { };
    auto* layoutBox = root.firstInFlowOrFloatingChild();
    while (layoutBox) {
        if (layoutBox->establishesFormattingContext()) {
            formattingContextRootList.append(layoutBox);
            computeIntrinsicWidthForFormattingRoot(*layoutBox);
        } else if (layoutBox->isReplaced() || is<Container>(*layoutBox)) {
            computeBorderAndPadding(*layoutBox, usedValues);
            // inline-block and replaced.
            auto needsWidthComputation = layoutBox->isReplaced();
            if (needsWidthComputation)
                computeWidthAndMargin(*layoutBox, usedValues);
            else {
                // Simple inline container with no intrinsic width <span>.
                computeHorizontalMargin(*layoutBox, usedValues);
            }
        }
        layoutBox = nextInPreOrder(*layoutBox, root);
    }

    collectInlineContent();

    auto maximumLineWidth = [&](auto availableWidth) {
        // Switch to the min/max formatting root width values before formatting the lines.
        for (auto* formattingRoot : formattingContextRootList) {
            auto intrinsicWidths = layoutState.formattingStateForBox(*formattingRoot).intrinsicWidthConstraintsForBox(*formattingRoot);
            auto& displayBox = displayBoxForLayoutBox(*formattingRoot);
            auto contentWidth = (availableWidth ? intrinsicWidths->maximum : intrinsicWidths->minimum) - displayBox.horizontalMarginBorderAndPadding();
            displayBox.setContentBoxWidth(contentWidth);
        }
        return InlineLayout(*this).computedIntrinsicWidth(formattingState().inlineItems(), availableWidth);
    };

    auto constraints = geometry().constrainByMinMaxWidth(root, { maximumLineWidth(0), maximumLineWidth(LayoutUnit::max()) });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

void InlineFormattingContext::initializeMarginBorderAndPaddingForGenericInlineBox(const Box& layoutBox)
{
    ASSERT(layoutBox.isAnonymous() || layoutBox.isLineBreakBox());
    auto& displayBox = displayBoxForLayoutBox(layoutBox);

    displayBox.setVerticalMargin({ { }, { } });
    displayBox.setHorizontalMargin({ });
    displayBox.setBorder({ { }, { } });
    displayBox.setPadding({ });
}

void InlineFormattingContext::computeMarginBorderAndPaddingForInlineContainer(const Container& container, UsedHorizontalValues usedValues)
{
    computeHorizontalMargin(container, usedValues);
    computeBorderAndPadding(container, usedValues);
    // Inline containers (<span>) have 0 vertical margins.
    displayBoxForLayoutBox(container).setVerticalMargin({ { }, { } });
}

void InlineFormattingContext::computeIntrinsicWidthForFormattingRoot(const Box& formattingRoot)
{
    ASSERT(formattingRoot.establishesFormattingContext());

    auto usedValues = UsedHorizontalValues { };
    computeBorderAndPadding(formattingRoot, usedValues);
    computeHorizontalMargin(formattingRoot, usedValues);

    IntrinsicWidthConstraints constraints;
    if (auto fixedWidth = geometry().fixedValue(formattingRoot.style().logicalWidth()))
        constraints = { *fixedWidth, *fixedWidth };
    else
        constraints = layoutState().createFormattingContext(formattingRoot)->computedIntrinsicWidthConstraints();
    constraints = geometry().constrainByMinMaxWidth(formattingRoot, constraints);
    constraints.expand(displayBoxForLayoutBox(formattingRoot).horizontalMarginBorderAndPadding());
    formattingState().setIntrinsicWidthConstraintsForBox(formattingRoot, constraints);
}

void InlineFormattingContext::computeHorizontalMargin(const Box& layoutBox, UsedHorizontalValues usedValues)
{
    auto computedHorizontalMargin = geometry().computedHorizontalMargin(layoutBox, usedValues);
    auto& displayBox = displayBoxForLayoutBox(layoutBox);
    displayBox.setHorizontalComputedMargin(computedHorizontalMargin);
    displayBox.setHorizontalMargin({ computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) });
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox, UsedHorizontalValues usedValues)
{
    WidthAndMargin widthAndMargin;
    if (layoutBox.isFloatingPositioned())
        widthAndMargin = geometry().floatingWidthAndMargin(layoutBox, usedValues);
    else if (layoutBox.isInlineBlockBox())
        widthAndMargin = geometry().inlineBlockWidthAndMargin(layoutBox, usedValues);
    else if (layoutBox.replaced())
        widthAndMargin = geometry().inlineReplacedWidthAndMargin(layoutBox, usedValues);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(widthAndMargin.computedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox)
{
    HeightAndMargin heightAndMargin;
    if (layoutBox.isFloatingPositioned())
        heightAndMargin = geometry().floatingHeightAndMargin(layoutBox, { }, UsedHorizontalValues { displayBoxForLayoutBox(*layoutBox.containingBlock()).contentBoxWidth() });
    else if (layoutBox.isInlineBlockBox())
        heightAndMargin = geometry().inlineBlockHeightAndMargin(layoutBox);
    else if (layoutBox.replaced())
        heightAndMargin = geometry().inlineReplacedHeightAndMargin(layoutBox, { });
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalMargin({ heightAndMargin.nonCollapsedMargin, { } });
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& root, UsedHorizontalValues usedValues)
{
    ASSERT(root.isFloatingPositioned() || root.isInlineBlockBox());
    ASSERT(usedValues.containingBlockWidth);

    computeBorderAndPadding(root, usedValues);
    computeWidthAndMargin(root, usedValues);
    // This is similar to static positioning in block formatting context. We just need to initialize the top left position.
    displayBoxForLayoutBox(root).setTopLeft({ 0, 0 });
    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutState().createFormattingContext(root);
    formattingContext->layout();
    // Come back and finalize the root's height and margin.
    computeHeightAndMargin(root);
    // Now that we computed the root's height, we can go back and layout the out-of-flow content.
    formattingContext->layoutOutOfFlowContent();
}

void InlineFormattingContext::computeWidthAndHeightForReplacedInlineBox(const Box& layoutBox, UsedHorizontalValues usedValues)
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());
    ASSERT(layoutBox.replaced());
    ASSERT(usedValues.containingBlockWidth);

    computeBorderAndPadding(layoutBox, usedValues);
    computeWidthAndMargin(layoutBox, usedValues);
    computeHeightAndMargin(layoutBox);
}

void InlineFormattingContext::collectInlineContent() const
{
    auto& root = downcast<Container>(this->root());
    // Traverse the tree and create inline items out of containers and leaf nodes. This essentially turns the tree inline structure into a flat one.
    // <span>text<span></span><img></span> -> [ContainerStart][InlineBox][ContainerStart][ContainerEnd][InlineBox][ContainerEnd]
    auto& formattingState = this->formattingState();
    LayoutQueue layoutQueue;
    layoutQueue.append(root.firstInFlowOrFloatingChild());
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

}
}

#endif
