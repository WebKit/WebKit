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
#include "Textutil.h"
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

    InlineRunProvider inlineRunProvider;
    collectInlineContent(inlineRunProvider);
    // Compute width/height for non-text content.
    for (auto& inlineRun : inlineRunProvider.runs()) {
        if (inlineRun.isText())
            continue;

        auto& layoutBox = inlineRun.inlineItem().layoutBox();
        if (layoutBox.establishesFormattingContext()) {
            layoutFormattingContextRoot(layoutBox);
            continue;
        }
        computeWidthAndHeightForReplacedInlineBox(layoutBox);
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
        auto floatConstraints = floatingState.constraints({ lineLogicalTop }, formattingRoot);
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

    line.init({ lineLogicalLeft, lineLogicalTop }, availableWidth, formattingRoot.style().computedLineHeight());
}

void InlineFormattingContext::splitInlineRunIfNeeded(const InlineRun& inlineRun, InlineRuns& splitRuns) const
{
    ASSERT(inlineRun.textContext());
    ASSERT(inlineRun.overlapsMultipleInlineItems());
    // In certain cases, a run can overlap multiple inline elements like this:
    // <span>normal text content</span><span style="position: relative; left: 10px;">but this one needs a dedicated run</span><span>end of text</span>
    // The content above generates one long run <normal text contentbut this one needs dedicated runend of text>
    // However, since the middle run is positioned, it needs to be moved independently from the rest of the content, hence it needs a dedicated inline run.

    // 1. Start with the first inline item (element) and travers the list until
    // 2. either find an inline item that needs a dedicated run or we reach the end of the run
    // 3. Create dedicate inline runs.
    auto& inlineContent = inlineFormattingState().inlineContent();
    auto contentStart = inlineRun.logicalLeft();
    auto startPosition = inlineRun.textContext()->start();
    auto remaningLength = inlineRun.textContext()->length();

    struct Uncommitted {
        const InlineItem* firstInlineItem { nullptr };
        const InlineItem* lastInlineItem { nullptr };
        unsigned length { 0 };
    };
    std::optional<Uncommitted> uncommitted;

    auto commit = [&] {
        if (!uncommitted)
            return;

        contentStart += uncommitted->firstInlineItem->nonBreakableStart();

        auto runWidth = Geometry::runWidth(inlineContent, *uncommitted->firstInlineItem, startPosition, uncommitted->length, contentStart);
        auto run = InlineRun { { inlineRun.logicalTop(), contentStart, runWidth, inlineRun.logicalHeight() }, *uncommitted->firstInlineItem };
        run.setTextContext({ startPosition, uncommitted->length });
        splitRuns.append(run);

        contentStart += runWidth + uncommitted->lastInlineItem->nonBreakableEnd();
        remaningLength -= uncommitted->length;

        startPosition = 0;
        uncommitted = { };
    };

    for (auto iterator = inlineContent.find(const_cast<InlineItem*>(&inlineRun.inlineItem())); iterator != inlineContent.end() && remaningLength > 0; ++iterator) {
        auto& inlineItem = **iterator;

        // Skip all non-inflow boxes (floats, out-of-flow positioned elements). They don't participate in the inline run context.
        if (!inlineItem.layoutBox().isInFlow())
            continue;

        auto currentLength = [&] {
            return std::min(remaningLength, inlineItem.textContent().length() - startPosition);
        };

        // 1. Break before/after -> requires dedicated run -> commit what we've got so far and also commit the current inline element as a separate inline run.
        // 2. Break at the beginning of the inline element -> commit what we've got so far. Current element becomes the first uncommitted.
        // 3. Break at the end of the inline element -> commit what we've got so far including the current element.
        // 4. Inline element does not require run breaking -> add current inline element to uncommitted. Jump to the next element.
        auto detachingRules = inlineItem.detachingRules();

        // #1
        if (detachingRules.containsAll({ InlineItem::DetachingRule::BreakAtStart, InlineItem::DetachingRule::BreakAtEnd })) {
            commit();
            uncommitted = Uncommitted { &inlineItem, &inlineItem, currentLength() };
            commit();
            continue;
        }

        // #2
        if (detachingRules.contains(InlineItem::DetachingRule::BreakAtStart))
            commit();

        // Add current inline item to uncommitted.
        // #3 and #4
        if (!uncommitted)
            uncommitted = Uncommitted { &inlineItem, &inlineItem, 0 };
        uncommitted->length += currentLength();
        uncommitted->lastInlineItem = &inlineItem;

        // #3
        if (detachingRules.contains(InlineItem::DetachingRule::BreakAtEnd))
            commit();
    }
    // Either all inline elements needed dedicated runs or neither of them.
    if (!remaningLength || remaningLength == inlineRun.textContext()->length())
        return;

    commit();
}

void InlineFormattingContext::createFinalRuns(Line& line) const
{
    auto& inlineFormattingState = this->inlineFormattingState();
    for (auto& inlineRun : line.runs()) {
        if (inlineRun.overlapsMultipleInlineItems()) {
            InlineRuns splitRuns;
            splitInlineRunIfNeeded(inlineRun, splitRuns);
            for (auto& splitRun : splitRuns)
                inlineFormattingState.appendInlineRun(splitRun);

            if (!splitRuns.isEmpty())
                continue;
        }

        auto finalRun = [&] {
            auto& inlineItem = inlineRun.inlineItem();
            if (inlineItem.detachingRules().isEmpty())
                return inlineRun;

            InlineRun adjustedRun = inlineRun;
            auto width = inlineRun.logicalWidth() - inlineItem.nonBreakableStart() - inlineItem.nonBreakableEnd();
            adjustedRun.setLogicalLeft(inlineRun.logicalLeft() + inlineItem.nonBreakableStart());
            adjustedRun.setLogicalWidth(width);
            return adjustedRun;
        };

        inlineFormattingState.appendInlineRun(finalRun());
    }
}

void InlineFormattingContext::postProcessInlineRuns(Line& line, IsLastLine isLastLine) const
{
    Geometry::alignRuns(root().style().textAlign(), line, isLastLine);
    auto firstRunIndex = inlineFormattingState().inlineRuns().size();
    createFinalRuns(line);

    placeInFlowPositionedChildren(firstRunIndex);
}

void InlineFormattingContext::closeLine(Line& line, IsLastLine isLastLine) const
{
    line.close();
    if (!line.hasContent())
        return;

    postProcessInlineRuns(line, isLastLine);
}

void InlineFormattingContext::appendContentToLine(Line& line, const InlineRunProvider::Run& run, const LayoutSize& runSize) const
{
    auto lastRunType = line.lastRunType();
    line.appendContent(run, runSize);

    if (root().style().textAlign() == TextAlignMode::Justify)
        Geometry::computeExpansionOpportunities(line, run, lastRunType.value_or(InlineRunProvider::Run::Type::NonWhitespace));
}

void InlineFormattingContext::layoutInlineContent(const InlineRunProvider& inlineRunProvider) const
{
    auto& layoutState = this->layoutState();
    auto& inlineFormattingState = this->inlineFormattingState();
    auto floatingContext = FloatingContext { inlineFormattingState.floatingState() };

    Line line;
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

            auto floatBoxWidth = layoutState.displayBoxForLayoutBox(floatBox).marginBox().width();
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
                    closeLine(line, IsLastLine::No);
                initializeNewLine(line);
            }
         }

        if (generatesInlineRun) {
            auto width = run->width;
            auto height = run->content.isText() ? LayoutUnit(root().style().computedLineHeight()) : layoutState.displayBoxForLayoutBox(run->content.inlineItem().layoutBox()).height(); 
            appendContentToLine(line, run->content, { width, height });
        }

        if (isLastRun)
            closeLine(line, IsLastLine::No);
    }

    closeLine(line, IsLastLine::Yes);
}

void InlineFormattingContext::computeWidthAndMargin(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    WidthAndMargin widthAndMargin;
    if (layoutBox.isFloatingPositioned())
        widthAndMargin = Geometry::floatingWidthAndMargin(layoutState, layoutBox);
    else if (layoutBox.isInlineBlockBox())
        widthAndMargin = Geometry::inlineBlockWidthAndMargin(layoutState, layoutBox);
    else if (layoutBox.replaced())
        widthAndMargin = Geometry::inlineReplacedWidthAndMargin(layoutState, layoutBox);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxWidth(widthAndMargin.width);
    displayBox.setHorizontalMargin(widthAndMargin.margin);
    displayBox.setHorizontalNonComputedMargin(widthAndMargin.nonComputedMargin);
}

void InlineFormattingContext::computeHeightAndMargin(const Box& layoutBox) const
{
    auto& layoutState = this->layoutState();

    HeightAndMargin heightAndMargin;
    if (layoutBox.isFloatingPositioned())
        heightAndMargin = Geometry::floatingHeightAndMargin(layoutState, layoutBox);
    else if (layoutBox.isInlineBlockBox())
        heightAndMargin = Geometry::inlineBlockHeightAndMargin(layoutState, layoutBox);
    else if (layoutBox.replaced())
        heightAndMargin = Geometry::inlineReplacedHeightAndMargin(layoutState, layoutBox);
    else
        ASSERT_NOT_REACHED();

    auto& displayBox = layoutState.displayBoxForLayoutBox(layoutBox);
    displayBox.setContentBoxHeight(heightAndMargin.height);
    displayBox.setVerticalNonCollapsedMargin(heightAndMargin.nonCollapsedMargin);
    displayBox.setVerticalMargin(heightAndMargin.usedMarginValues());
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& root) const
{
    ASSERT(root.isFloatingPositioned() || root.isInlineBlockBox());

    computeBorderAndPadding(root);
    computeWidthAndMargin(root);
    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutState().createFormattingStateForFormattingRootIfNeeded(root).createFormattingContext(root);
    formattingContext->layout();
    // Come back and finalize the root's height and margin.
    computeHeightAndMargin(root);
    // Now that we computed the root's height, we can go back and layout the out-of-flow descedants (if any).
    formattingContext->layoutOutOfFlowDescendants(root);
}

void InlineFormattingContext::computeWidthAndHeightForReplacedInlineBox(const Box& layoutBox) const
{
    ASSERT(!layoutBox.isContainer());
    ASSERT(!layoutBox.establishesFormattingContext());
    ASSERT(layoutBox.replaced());

    computeBorderAndPadding(layoutBox);
    computeWidthAndMargin(layoutBox);
    computeHeightAndMargin(layoutBox);
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

void InlineFormattingContext::placeInFlowPositionedChildren(unsigned fistRunIndex) const
{
    auto& inlineRuns = inlineFormattingState().inlineRuns();
    for (auto runIndex = fistRunIndex; runIndex < inlineRuns.size(); ++runIndex) {
        auto& inlineRun = inlineRuns[runIndex];

        auto positionOffset = [&](auto& layoutBox) {
            // FIXME: Need to figure out whether in-flow offset should stick. This might very well be temporary.
            std::optional<LayoutSize> offset;
            for (auto* box = &layoutBox; box != &root(); box = box->parent()) {
                if (!box->isInFlowPositioned())
                    continue;
                offset = offset.value_or(LayoutSize()) + Geometry::inFlowPositionedPositionOffset(layoutState(), *box);
            }
            return offset;
        };

        if (auto offset = positionOffset(inlineRun.inlineItem().layoutBox())) {
            inlineRun.moveVertically(offset->height());
            inlineRun.moveHorizontally(offset->width());
        }
    }
}

void InlineFormattingContext::collectInlineContentForSubtree(const Box& root, InlineRunProvider& inlineRunProvider) const
{
    // Collect inline content recursively and set breaking rules for the inline elements (for paddings, margins, positioned element etc).
    auto& inlineFormattingState = this->inlineFormattingState();

    auto createAndAppendInlineItem = [&] {
        auto inlineItem = std::make_unique<InlineItem>(root);
        inlineRunProvider.append(*inlineItem);
        inlineFormattingState.inlineContent().add(WTFMove(inlineItem));
    };

    if (root.establishesFormattingContext() && &root != &(this->root())) {
        createAndAppendInlineItem();
        auto& inlineRun = *inlineFormattingState.inlineContent().last();

        auto horizontalMargins = Geometry::computedNonCollapsedHorizontalMarginValue(layoutState(), root);
        inlineRun.addDetachingRule({ InlineItem::DetachingRule::BreakAtStart, InlineItem::DetachingRule::BreakAtEnd });
        inlineRun.addNonBreakableStart(horizontalMargins.left);
        inlineRun.addNonBreakableEnd(horizontalMargins.right);
        // Skip formatting root subtree. They are not part of this inline formatting context.
        return;
    }

    if (!is<Container>(root)) {
        createAndAppendInlineItem();
        return;
    }

    auto* lastInlineBoxBeforeContainer = inlineFormattingState.lastInlineItem();
    auto* child = downcast<Container>(root).firstInFlowOrFloatingChild();
    while (child) {
        collectInlineContentForSubtree(*child, inlineRunProvider);
        child = child->nextInFlowOrFloatingSibling();
    }

    // FIXME: Revisit this when we figured out how inline boxes fit the display tree.
    auto padding = Geometry::computedPadding(layoutState(), root);
    auto border = Geometry::computedBorder(layoutState(), root);
    auto horizontalMargins = Geometry::computedNonCollapsedHorizontalMarginValue(layoutState(), root);
    // Setup breaking boundaries for this subtree.
    auto* lastDescendantInlineBox = inlineFormattingState.lastInlineItem();
    // Empty container?
    if (lastInlineBoxBeforeContainer == lastDescendantInlineBox)
        return;

    auto rootBreaksAtStart = [&] {
        if (&root == &(this->root()))
            return false;
        return (padding && padding->horizontal.left) || border.horizontal.left || horizontalMargins.left || root.isPositioned();
    };

    auto rootBreaksAtEnd = [&] {
        if (&root == &(this->root()))
            return false;
        return (padding && padding->horizontal.right) || border.horizontal.right || horizontalMargins.right || root.isPositioned();
    };

    if (rootBreaksAtStart()) {
        InlineItem* firstDescendantInlineBox = nullptr;
        auto& inlineContent = inlineFormattingState.inlineContent();

        if (lastInlineBoxBeforeContainer) {
            auto iterator = inlineContent.find(lastInlineBoxBeforeContainer);
            firstDescendantInlineBox = (*++iterator).get();
        } else
            firstDescendantInlineBox = inlineContent.first().get();

        ASSERT(firstDescendantInlineBox);
        firstDescendantInlineBox->addDetachingRule(InlineItem::DetachingRule::BreakAtStart);
        auto startOffset = border.horizontal.left + horizontalMargins.left;
        if (padding)
            startOffset += padding->horizontal.left;
        firstDescendantInlineBox->addNonBreakableStart(startOffset);
    }

    if (rootBreaksAtEnd()) {
        lastDescendantInlineBox->addDetachingRule(InlineItem::DetachingRule::BreakAtEnd);
        auto endOffset = border.horizontal.right + horizontalMargins.right;
        if (padding)
            endOffset += padding->horizontal.right;
        lastDescendantInlineBox->addNonBreakableEnd(endOffset);
    }
}

void InlineFormattingContext::collectInlineContent(InlineRunProvider& inlineRunProvider) const
{
    collectInlineContentForSubtree(root(), inlineRunProvider);
}

FormattingContext::InstrinsicWidthConstraints InlineFormattingContext::instrinsicWidthConstraints() const
{
    auto& formattingStateForRoot = layoutState().formattingStateForBox(root());
    if (auto instrinsicWidthConstraints = formattingStateForRoot.instrinsicWidthConstraints(root()))
        return *instrinsicWidthConstraints;

    auto& inlineFormattingState = this->inlineFormattingState();
    InlineRunProvider inlineRunProvider;
    collectInlineContent(inlineRunProvider);

    // Compute width for non-text content.
    for (auto& inlineRun : inlineRunProvider.runs()) {
        if (inlineRun.isText())
            continue;

        computeWidthAndMargin(inlineRun.inlineItem().layoutBox());
    }

    auto maximumLineWidth = [&](auto availableWidth) {
        LayoutUnit maxContentLogicalRight;
        InlineLineBreaker lineBreaker(layoutState(), inlineFormattingState.inlineContent(), inlineRunProvider.runs());
        LayoutUnit lineLogicalRight;
        while (auto run = lineBreaker.nextRun(lineLogicalRight, availableWidth, !lineLogicalRight)) {
            if (run->position == InlineLineBreaker::Run::Position::LineBegin)
                lineLogicalRight = 0;
            lineLogicalRight += run->width;

            maxContentLogicalRight = std::max(maxContentLogicalRight, lineLogicalRight);
        }
        return maxContentLogicalRight;
    };

    return FormattingContext::InstrinsicWidthConstraints { maximumLineWidth(0), maximumLineWidth(LayoutUnit::max()) };
}

}
}

#endif
