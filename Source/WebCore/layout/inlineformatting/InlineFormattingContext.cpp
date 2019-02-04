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
    auto& inlineContent = formattingState().inlineContent();
    auto contentStart = inlineRun.logicalLeft();
    auto startPosition = inlineRun.textContext()->start();
    auto remaningLength = inlineRun.textContext()->length();

    struct Uncommitted {
        const InlineItem* firstInlineItem { nullptr };
        const InlineItem* lastInlineItem { nullptr };
        unsigned length { 0 };
    };
    Optional<Uncommitted> uncommitted;

    auto commit = [&] {
        if (!uncommitted)
            return;

        contentStart += uncommitted->firstInlineItem->nonBreakableStart();

        auto runWidth = Geometry::runWidth(inlineContent, *uncommitted->firstInlineItem, startPosition, uncommitted->length, contentStart);
        auto run = InlineRun { { inlineRun.logicalTop(), contentStart, runWidth, inlineRun.logicalHeight() }, *uncommitted->firstInlineItem };
        run.setTextContext({ startPosition, uncommitted->length });
        splitRuns.append(run);

        contentStart += runWidth + uncommitted->lastInlineItem->nonBreakableEnd();

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
            auto contentLength = currentLength();
            uncommitted = Uncommitted { &inlineItem, &inlineItem, contentLength };
            remaningLength -= contentLength;
            commit();
            continue;
        }

        // #2
        if (detachingRules.contains(InlineItem::DetachingRule::BreakAtStart))
            commit();

        // Add current inline item to uncommitted.
        // #3 and #4
        auto contentLength = currentLength();
        if (!uncommitted)
            uncommitted = Uncommitted { &inlineItem, &inlineItem, 0 };
        uncommitted->length += contentLength;
        uncommitted->lastInlineItem = &inlineItem;
        remaningLength -= contentLength;

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
    auto& inlineFormattingState = formattingState();
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
    auto firstRunIndex = formattingState().inlineRuns().size();
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
        Geometry::computeExpansionOpportunities(line, run, lastRunType.valueOr(InlineRunProvider::Run::Type::NonWhitespace));
}

void InlineFormattingContext::layoutInlineContent(const InlineRunProvider& inlineRunProvider) const
{
    auto& layoutState = this->layoutState();
    auto& inlineFormattingState = formattingState();
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
    displayBox.setHorizontalMargin(widthAndMargin.usedMargin);
    displayBox.setHorizontalComputedMargin(widthAndMargin.computedMargin);
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
    displayBox.setVerticalMargin({ heightAndMargin.nonCollapsedMargin, { } });
}

void InlineFormattingContext::layoutFormattingContextRoot(const Box& root) const
{
    ASSERT(root.isFloatingPositioned() || root.isInlineBlockBox());

    computeBorderAndPadding(root);
    computeWidthAndMargin(root);
    // Swich over to the new formatting context (the one that the root creates).
    auto formattingContext = layoutState().createFormattingContext(root);
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
    auto& inlineRuns = formattingState().inlineRuns();
    for (auto runIndex = fistRunIndex; runIndex < inlineRuns.size(); ++runIndex) {
        auto& inlineRun = inlineRuns[runIndex];

        auto positionOffset = [&](auto& layoutBox) {
            // FIXME: Need to figure out whether in-flow offset should stick. This might very well be temporary.
            Optional<LayoutSize> offset;
            for (auto* box = &layoutBox; box != &root(); box = box->parent()) {
                if (!box->isInFlowPositioned())
                    continue;
                offset = offset.valueOr(LayoutSize()) + Geometry::inFlowPositionedPositionOffset(layoutState(), *box);
            }
            return offset;
        };

        if (auto offset = positionOffset(inlineRun.inlineItem().layoutBox())) {
            inlineRun.moveVertically(offset->height());
            inlineRun.moveHorizontally(offset->width());
        }
    }
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
        auto computedHorizontalMargin = Geometry::computedHorizontalMargin(layoutState, container);
        auto horizontalMargin = UsedHorizontalMargin { computedHorizontalMargin.start.valueOr(0), computedHorizontalMargin.end.valueOr(0) };
        auto border = Geometry::computedBorder(layoutState, container);
        auto padding = Geometry::computedPadding(layoutState, container);

        if (type == NonBreakableWidthType::Start)
            return border.horizontal.left + horizontalMargin.start + (padding ? padding->horizontal.left : LayoutUnit());
        return border.horizontal.right + horizontalMargin.end + (padding ? padding->horizontal.right : LayoutUnit());
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
                auto computedHorizontalMargin = Geometry::computedHorizontalMargin(layoutState, container);
                auto currentNonBreakableStartWidth = nonBreakableStartWidth.valueOr(0) + computedHorizontalMargin.start.valueOr(0) + nonBreakableEndWidth.valueOr(0);
                addDetachingRules(inlineItem, currentNonBreakableStartWidth, computedHorizontalMargin.end);
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

FormattingContext::InstrinsicWidthConstraints InlineFormattingContext::instrinsicWidthConstraints() const
{
    auto& formattingStateForRoot = layoutState().formattingStateForBox(root());
    if (auto instrinsicWidthConstraints = formattingStateForRoot.instrinsicWidthConstraints(root()))
        return *instrinsicWidthConstraints;

    auto& inlineFormattingState = formattingState();
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
