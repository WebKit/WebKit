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

#include "FloatingState.h"
#include "InlineFormattingState.h"
#include "InlineLineBreaker.h"
#include "InlineRunProvider.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutContext.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Box& formattingContextRoot)
    : FormattingContext(formattingContextRoot)
{
}

void InlineFormattingContext::layout(LayoutContext& layoutContext, FormattingState& formattingState) const
{
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");

    auto& inlineFormattingState = downcast<InlineFormattingState>(formattingState);
    InlineRunProvider inlineRunProvider(inlineFormattingState);
    auto& formattingRoot = downcast<Container>(root());
    auto* layoutBox = formattingRoot.firstInFlowOrFloatingChild();
    // Casually walk through the block's descendants and place the inline boxes one after the other as much as we can (yeah, I am looking at you floats).
    while (layoutBox) {
        if (is<Container>(layoutBox)) {
            ASSERT(is<InlineContainer>(layoutBox));
            layoutBox = downcast<Container>(*layoutBox).firstInFlowOrFloatingChild();
            continue;
        }

        inlineRunProvider.append(*layoutBox);
        computeWidthAndHeight(layoutContext, *layoutBox);

        for (; layoutBox; layoutBox = layoutBox->parent()) {
            if (layoutBox == &formattingRoot) {
                layoutBox = nullptr;
                break;
            }
            if (auto* nextSibling = layoutBox->nextInFlowOrFloatingSibling()) {
                layoutBox = nextSibling;
                break;
            }
        }
        ASSERT(!layoutBox || layoutBox->isDescendantOf(formattingRoot));
    }

    layoutInlineContent(layoutContext, inlineFormattingState, inlineRunProvider);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> layout context(" << &layoutContext << ") formatting root(" << &root() << ")");
}

static bool trimLeadingRun(const InlineLineBreaker::Run& run)
{
    ASSERT(run.position == InlineLineBreaker::Run::Position::LineBegin);

    if (!run.content.isWhitespace())
        return false;

    return run.content.style().collapseWhiteSpace();
}

void InlineFormattingContext::layoutInlineContent(const LayoutContext& layoutContext, InlineFormattingState& inlineFormattingState, const InlineRunProvider& inlineRunProvider) const
{
    auto& formattingRoot = downcast<Container>(root());
    auto& formattingRootDisplayBox = layoutContext.displayBoxForLayoutBox(formattingRoot);

    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();
    auto availableWidth = formattingRootDisplayBox.contentBoxWidth();
    auto previousRunPositionIsLineEnd = false;

    Line line(inlineFormattingState, root());
    line.init(lineLogicalLeft, availableWidth);

    InlineLineBreaker lineBreaker(layoutContext, inlineFormattingState.inlineContent(), inlineRunProvider.runs());
    while (auto run = lineBreaker.nextRun(line.contentLogicalRight(), line.availableWidth(), !line.hasContent())) {

        if (run->position == InlineLineBreaker::Run::Position::LineBegin) {
            // First run on line.

            // Previous run ended up being at the line end. Adjust the line accordingly.
            if (!previousRunPositionIsLineEnd) {
                line.close();
                line.init(lineLogicalLeft, availableWidth);
            }
            // Skip leading whitespace.
            if (!trimLeadingRun(*run))
                line.appendContent(*run);
            continue;
        }

        if (run->position == InlineLineBreaker::Run::Position::LineEnd) {
            // Last run on line.
            previousRunPositionIsLineEnd = true;
            line.appendContent(*run);
            // Move over to the next line.
            line.close();
            line.init(lineLogicalLeft, availableWidth);
            continue;
        }

        // This may or may not be the last run on line -but definitely not the first one.
        line.appendContent(*run);
    }
    line.close();
}

void InlineFormattingContext::computeWidthAndHeight(const LayoutContext& layoutContext, const Box& layoutBox) const
{
    if (is<InlineBox>(layoutBox) && downcast<InlineBox>(layoutBox).hasTextContent()) {
        // Text content width is computed during text run generation. -It does not make any sense to measure unprocessed text here, since it will likely be
        // split up (or concatenated).
        return;
    }

    if (layoutBox.isFloatingPositioned()) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (layoutBox.isInlineBlockBox()) {
        ASSERT_NOT_IMPLEMENTED_YET();
        return;
    }

    if (layoutBox.replaced()) {
        computeBorderAndPadding(layoutContext, layoutBox);

        auto& displayBox = layoutContext.displayBoxForLayoutBox(layoutBox);

        auto widthAndMargin = Geometry::inlineReplacedWidthAndMargin(layoutContext, layoutBox);
        displayBox.setContentBoxWidth(widthAndMargin.width);
        displayBox.setHorizontalMargin(widthAndMargin.margin);
        displayBox.setHorizontalNonComputedMargin(widthAndMargin.nonComputedMargin);

        auto heightAndMargin = Geometry::inlineReplacedHeightAndMargin(layoutContext, layoutBox);
        displayBox.setContentBoxHeight(heightAndMargin.height);
        displayBox.setVerticalNonCollapsedMargin(heightAndMargin.margin);
        displayBox.setVerticalMargin(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin));
        return;
    }

    ASSERT_NOT_REACHED();
    return;
}

void InlineFormattingContext::computeStaticPosition(const LayoutContext&, const Box&) const
{
}

void InlineFormattingContext::computeInFlowPositionedPosition(const LayoutContext&, const Box&) const
{
}

FormattingContext::InstrinsicWidthConstraints InlineFormattingContext::instrinsicWidthConstraints(LayoutContext&, const Box&) const
{
    return { };
}

}
}

#endif
