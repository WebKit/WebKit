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
        else if (is<Container>(*layoutBox))
            computeMarginBorderAndPaddingForInlineContainer(downcast<Container>(*layoutBox), usedHorizontalValues);
        else if (layoutBox->isReplaced())
            computeWidthAndHeightForReplacedInlineBox(*layoutBox, usedHorizontalValues, usedVerticalValues);
        else {
            ASSERT(layoutBox->isInlineLevelBox());
            initializeMarginBorderAndPaddingForGenericInlineBox(*layoutBox);
        }
        layoutBox = nextInPreOrder(*layoutBox, root());
    }

    // FIXME: This is such a waste when intrinsic width computation already collected the inline items.
    formattingState().inlineItems().clear();
    formattingState().inlineRuns().clear();

    collectInlineContent();
    InlineLayout(*this, usedHorizontalValues).layout(formattingState().inlineItems());
    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
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
        return InlineLayout(*this, usedHorizontalValues).computedIntrinsicWidth(formattingState().inlineItems());
    };

    auto constraints = geometry().constrainByMinMaxWidth(root(), { maximumLineWidth(0), maximumLineWidth(LayoutUnit::max()) });
    formattingState().setIntrinsicWidthConstraints(constraints);
    return constraints;
}

void InlineFormattingContext::initializeMarginBorderAndPaddingForGenericInlineBox(const Box& layoutBox)
{
    ASSERT(layoutBox.isAnonymous() || layoutBox.isLineBreakBox());
    auto& displayBox = formattingState().displayBox(layoutBox);

    displayBox.setVerticalMargin({ { }, { } });
    displayBox.setHorizontalMargin({ });
    displayBox.setBorder({ { }, { } });
    displayBox.setPadding({ });
}

void InlineFormattingContext::computeMarginBorderAndPaddingForInlineContainer(const Container& container, UsedHorizontalValues usedHorizontalValues)
{
    computeHorizontalMargin(container, usedHorizontalValues);
    computeBorderAndPadding(container, usedHorizontalValues);
    // Inline containers (<span>) have 0 vertical margins.
    formattingState().displayBox(container).setVerticalMargin({ { }, { } });
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

void InlineFormattingContext::layoutFormattingContextRoot(const Box& root, UsedHorizontalValues usedHorizontalValues, UsedVerticalValues usedVerticalValues)
{
    ASSERT(root.isFloatingPositioned() || root.isInlineBlockBox());

    computeBorderAndPadding(root, usedHorizontalValues);
    computeWidthAndMargin(root, usedHorizontalValues);
    // This is similar to static positioning in block formatting context. We just need to initialize the top left position.
    formattingState().displayBox(root).setTopLeft({ 0, 0 });
    // Swich over to the new formatting context (the one that the root creates).
    if (is<Container>(root)) {
        auto& rootContainer = downcast<Container>(root);
        auto formattingContext = LayoutContext::createFormattingContext(rootContainer, layoutState());
        formattingContext->layoutInFlowContent();
        // Come back and finalize the root's height and margin.
        computeHeightAndMargin(rootContainer, usedHorizontalValues, usedVerticalValues);
        // Now that we computed the root's height, we can go back and layout the out-of-flow content.
        formattingContext->layoutOutOfFlowContent();
    } else
        computeHeightAndMargin(root, usedHorizontalValues, usedVerticalValues);
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

}
}

#endif
