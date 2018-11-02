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
#include "LayoutFormattingState.h"
#include "LayoutInlineBox.h"
#include "LayoutInlineContainer.h"
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(InlineFormattingContext);

InlineFormattingContext::InlineFormattingContext(const Box& formattingContextRoot, FormattingState& formattingState)
    : FormattingContext(formattingContextRoot, formattingState)
{
}

void InlineFormattingContext::layout() const
{
    if (!is<Container>(root()))
        return;

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[Start] -> inline formatting context -> formatting root(" << &root() << ")");

    auto& inlineFormattingState = downcast<InlineFormattingState>(formattingState());
    InlineRunProvider inlineRunProvider(inlineFormattingState);
    auto& formattingRoot = downcast<Container>(root());
    auto* layoutBox = formattingRoot.firstInFlowOrFloatingChild();
    // Casually walk through the block's descendants and place the inline boxes one after the other as much as we can (yeah, I am looking at you floats).
    while (layoutBox) {

        if (layoutBox->establishesFormattingContext()) {
            layoutFormattingContextRoot(*layoutBox);
            // Formatting context roots take care of their entire subtree. Continue with next sibling.
            inlineRunProvider.append(*layoutBox);
            layoutBox = layoutBox->nextInFlowOrFloatingSibling();
            continue;
        }

        if (is<Container>(layoutBox)) {
            ASSERT(is<InlineContainer>(layoutBox));
            layoutBox = downcast<Container>(*layoutBox).firstInFlowOrFloatingChild();
            continue;
        }

        inlineRunProvider.append(*layoutBox);
        computeWidthAndHeightForInlineBox(*layoutBox);

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

    layoutInlineContent(inlineRunProvider);

    LOG_WITH_STREAM(FormattingContextLayout, stream << "[End] -> inline formatting context -> formatting root(" << &root() << ")");
}

static bool isTrimmableContent(const InlineLineBreaker::Run& run)
{
    return run.content.isWhitespace() && run.content.style().collapseWhiteSpace();
}

void InlineFormattingContext::initializeNewLine(Line& line) const
{
    auto& formattingRoot = downcast<Container>(root());
    auto& formattingRootDisplayBox = layoutState().displayBoxForLayoutBox(formattingRoot);

    auto lineLogicalLeft = formattingRootDisplayBox.contentBoxLeft();
    auto lineLogicalTop = line.isFirstLine() ? formattingRootDisplayBox.contentBoxTop() : line.logicalBottom();
    auto availableWidth = formattingRootDisplayBox.contentBoxWidth();

    // Check for intruding floats and adjust logical left/available width for this line accordingly.
    auto& floatingState = formattingState().floatingState();
    if (!floatingState.isEmpty()) {
        auto floatConstraints = floatingState.constraints(lineLogicalTop, formattingRoot);
        // Check if these constraints actually put limitation on the line.
        if (floatConstraints.left && *floatConstraints.left <= formattingRootDisplayBox.contentBoxLeft())
            floatConstraints.left = { };

        if (floatConstraints.right && *floatConstraints.right >= formattingRootDisplayBox.contentBoxRight())
            floatConstraints.right = { };

        if (floatConstraints.left && floatConstraints.right) {
            ASSERT(*floatConstraints.left < *floatConstraints.right);
            availableWidth = *floatConstraints.right - *floatConstraints.left;
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.left) {
            ASSERT(*floatConstraints.left > lineLogicalLeft);
            availableWidth -= (*floatConstraints.left - lineLogicalLeft);
            lineLogicalLeft = *floatConstraints.left;
        } else if (floatConstraints.right) {
            ASSERT(*floatConstraints.right > lineLogicalLeft);
            availableWidth = *floatConstraints.right - lineLogicalLeft;
        }
    }

    Display::Box::Rect logicalRect;
    logicalRect.setTop(lineLogicalTop);
    logicalRect.setLeft(lineLogicalLeft);
    logicalRect.setWidth(availableWidth);
    logicalRect.setHeight(formattingRoot.style().computedLineHeight());

    line.init(logicalRect);
}

void InlineFormattingContext::layoutInlineContent(const InlineRunProvider& inlineRunProvider) const
{
    auto& layoutState = this->layoutState();
    auto& inlineFormattingState = downcast<InlineFormattingState>(formattingState());
    auto floatingContext = FloatingContext { inlineFormattingState.floatingState() };

    Line line(inlineFormattingState, root());
    initializeNewLine(line);

    InlineLineBreaker lineBreaker(layoutState, inlineFormattingState.inlineContent(), inlineRunProvider.runs());
    while (auto run = lineBreaker.nextRun(line.contentLogicalRight(), line.availableWidth(), !line.hasContent())) {
        auto isFirstRun = run->position == InlineLineBreaker::Run::Position::LineBegin;
        auto isLastRun = run->position == InlineLineBreaker::Run::Position::LineEnd;
        auto generatesInlineRun = true;

        // Position float and adjust the runs on line.
        if (run->content.isFloat()) {
            auto& floatBox = run->content.inlineItem().layoutBox();
            computeFloatPosition(floatingContext, line, floatBox);
            inlineFormattingState.floatingState().append(floatBox);

            auto floatBoxWidth = layoutState.displayBoxForLayoutBox(floatBox).width();
            // Shrink availble space for current line and move existing inline runs.
            floatBox.isLeftFloatingPositioned() ? line.adjustLogicalLeft(floatBoxWidth) : line.adjustLogicalRight(floatBoxWidth);

            generatesInlineRun = false;
        }

        // 1. Initialize new line if needed.
        // 2. Append inline run unless it is skipped.
        // 3. Close current line if needed.
        if (isFirstRun) {
            // When the first run does not generate an actual inline run, the next run comes in first-run as well.
            // No need to spend time on closing/initializing.
            // Skip leading whitespace.
            if (!generatesInlineRun || isTrimmableContent(*run))
                continue;

            if (line.hasContent()) {
                // Previous run ended up being at the line end. Adjust the line accordingly.
                if (!line.isClosed())
                    line.close(Line::LastLine::No);
                initializeNewLine(line);
            }
         }

        if (generatesInlineRun)
             line.appendContent(*run);

        if (isLastRun)
            line.close(Line::LastLine::No);
    }

    line.close(Line::LastLine::Yes);
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    ASSERT(layoutBox.isFloatingPositioned() || layoutBox.isInlineBlockBox());
    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);

    auto computeWidthAndMargin = [&]() {
        WidthAndMargin widthAndMargin;

        if (layoutBox.isFloatingPositioned())
            widthAndMargin = Geometry::floatingWidthAndMargin(layoutState, layoutBox);
        else if (layoutBox.isInlineBlockBox())
            widthAndMargin = Geometry::inlineBlockWidthAndMargin(layoutState, layoutBox);
        else
            ASSERT_NOT_REACHED();

        displayBox.setContentBoxWidth(widthAndMargin.width);
        displayBox.setHorizontalMargin(widthAndMargin.margin);
        displayBox.setHorizontalNonComputedMargin(widthAndMargin.nonComputedMargin);
    };

    auto computeHeightAndMargin = [&]() {
        HeightAndMargin heightAndMargin;

        if (layoutBox.isFloatingPositioned())
            heightAndMargin = Geometry::floatingHeightAndMargin(layoutState, layoutBox);
        else if (layoutBox.isInlineBlockBox())
            heightAndMargin = Geometry::inlineBlockHeightAndMargin(layoutState, layoutBox);
        else
            ASSERT_NOT_REACHED();

        displayBox.setContentBoxHeight(heightAndMargin.height);
        displayBox.setVerticalNonCollapsedMargin(heightAndMargin.margin);
        displayBox.setVerticalMargin(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin));
    };

    layoutState.createFormattingStateForFormattingRootIfNeeded(layoutBox);
    computeBorderAndPadding(layoutBox);
    computeWidthAndMargin();

    // Swich over to the new formatting context (the one that the root creates).
    layoutState.establishedFormattingState(layoutBox).formattingContext(layoutBox)->layout();

    // Come back and finalize the root's height and margin.
    computeHeightAndMargin();
}

void InlineFormattingContext::computeWidthAndHeightForInlineBox(const Box& layoutBox) const
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());

    if (is<InlineBox>(layoutBox) && downcast<InlineBox>(layoutBox).hasTextContent()) {
        // Text content width is computed during text run generation. -It does not make any sense to measure unprocessed text here, since it will likely be
        // split up (or concatenated).
        return;
    }

    auto& layoutState = this->layoutState();
    // This is pretty much only for replaced inline boxes atm.
    ASSERT(layoutBox.replaced());
    computeBorderAndPadding(layoutBox);

    auto widthAndMargin = Geometry::inlineReplacedWidthAndMargin(layoutState, layoutBox);
    auto heightAndMargin = Geometry::inlineReplacedHeightAndMargin(layoutState, layoutBox);

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.margin);
    displayBox.setHorizontalNonComputedMargin(widthAndMargin.nonComputedMargin);

    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalNonCollapsedMargin(heightAndMargin.margin);
    displayBox.setVerticalMargin(heightAndMargin.collapsedMargin.value_or(heightAndMargin.margin));
}

void InlineFormattingContext::computeFloatPosition(const FloatingContext& floatingContext, Line& line, const Box& floatBox) const
{
    auto& layoutState = this->layoutState();
    ASSERT(layoutState.hasDisplayBox(floatBox));
    auto& displayBox = layoutState.displayBoxForLayoutBox(floatBox);

    // Set static position first.
    displayBox.setTopLeft({ line.contentLogicalRight(), line.logicalTop() });
    // Float it.
    displayBox.setTopLeft(floatingContext.positionForFloat(floatBox));
}

void InlineFormattingContext::computeStaticPosition(const Box&) const
{
}

void InlineFormattingContext::computeInFlowPositionedPosition(const Box&) const
{
}

FormattingContext::InstrinsicWidthConstraints InlineFormattingContext::instrinsicWidthConstraints() const
{
    return { };
}

}
}

#endif
