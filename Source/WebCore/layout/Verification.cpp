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
#include "LayoutState.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "InlineTextBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutTreeBuilder.h"
#include "RenderBox.h"
#include "RenderInline.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static bool areEssentiallyEqual(float a, LayoutUnit b)
{
    if (a == b.toFloat())
        return true;

    return fabs(a - b.toFloat()) <= 4 * LayoutUnit::epsilon();
}

static bool outputMismatchingSimpleLineInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBlockFlow& blockFlow, const Container& inlineFormattingRoot)
{
    auto* lineLayoutData = blockFlow.simpleLineLayout();
    if (!lineLayoutData) {
        ASSERT_NOT_REACHED();
        return true;
    }

    auto& inlineFormattingState = layoutState.establishedFormattingState(inlineFormattingRoot);
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    auto& inlineRunList = downcast<InlineFormattingState>(inlineFormattingState).inlineRuns();

    if (inlineRunList.size() != lineLayoutData->runCount()) {
        stream << "Mismatching number of runs: simple runs(" << lineLayoutData->runCount() << ") inline runs(" << inlineRunList.size() << ")";
        stream.nextLine();
        return true;
    }

    auto mismatched = false;
    for (unsigned i = 0; i < lineLayoutData->runCount(); ++i) {
        auto& simpleRun = lineLayoutData->runAt(i);
        auto& inlineRun = inlineRunList[i];

        auto matchingRuns = areEssentiallyEqual(simpleRun.logicalLeft, inlineRun.logicalLeft()) && areEssentiallyEqual(simpleRun.logicalRight, inlineRun.logicalRight());
        if (matchingRuns)
            matchingRuns = (simpleRun.start == inlineRun.textContext()->start() && simpleRun.end == (inlineRun.textContext()->start() + inlineRun.textContext()->length()));
        if (matchingRuns)
            continue;

        stream << "Mismatching: simple run(" << simpleRun.start << ", " << simpleRun.end << ") (" << simpleRun.logicalLeft << ", " << simpleRun.logicalRight << ") layout run(" << inlineRun.textContext()->start() << ", " << inlineRun.textContext()->start() + inlineRun.textContext()->length() << ") (" << inlineRun.logicalLeft() << ", " << inlineRun.logicalRight() << ")";
        stream.nextLine();
        mismatched = true;
    }
    return mismatched;
}

static bool checkForMatchingNonTextRuns(const InlineRun& inlineRun, const WebCore::InlineBox& inlineBox)
{
    return areEssentiallyEqual(inlineBox.logicalLeft(), inlineRun.logicalLeft())
        && areEssentiallyEqual(inlineBox.logicalRight(), inlineRun.logicalRight())
        && areEssentiallyEqual(inlineBox.logicalHeight(), inlineRun.logicalHeight());
}

static bool checkForMatchingTextRuns(const InlineRun& inlineRun, float logicalLeft, float logicalRight, unsigned start, unsigned end, float logicalHeight)
{
    return areEssentiallyEqual(logicalLeft, inlineRun.logicalLeft())
        && areEssentiallyEqual(logicalRight, inlineRun.logicalRight())
        && start == inlineRun.textContext()->start()
        && (end == (inlineRun.textContext()->start() + inlineRun.textContext()->length()))
        && areEssentiallyEqual(logicalHeight, inlineRun.logicalHeight());
}

static void collectFlowBoxSubtree(const InlineFlowBox& flowbox, Vector<WebCore::InlineBox*>& inlineBoxes)
{
    auto* inlineBox = flowbox.firstLeafChild();
    auto* lastLeafChild = flowbox.lastLeafChild();
    while (inlineBox) {
        inlineBoxes.append(inlineBox);
        if (inlineBox == lastLeafChild)
            break;
        inlineBox = inlineBox->nextLeafChild();
    }
}

static void collectInlineBoxes(const RenderBlockFlow& root, Vector<WebCore::InlineBox*>& inlineBoxes)
{
    for (auto* rootLine = root.firstRootBox(); rootLine; rootLine = rootLine->nextRootBox()) {
        for (auto* inlineBox = rootLine->firstChild(); inlineBox; inlineBox = inlineBox->nextOnLine()) {
            if (!is<InlineFlowBox>(inlineBox)) {
                inlineBoxes.append(inlineBox);
                continue;
            }
            collectFlowBoxSubtree(downcast<InlineFlowBox>(*inlineBox), inlineBoxes);
        }
    }
}

static LayoutUnit resolveForRelativePositionIfNeeded(const InlineTextBox& inlineTextBox)
{
    LayoutUnit xOffset;
    auto* parent = inlineTextBox.parent();
    while (is<InlineFlowBox>(parent)) {
        auto& renderer = parent->renderer();
        if (renderer.isInFlowPositioned())
            xOffset = downcast<RenderInline>(renderer).offsetForInFlowPosition().width();
        parent = parent->parent();
    }
    return xOffset;
}

static bool outputMismatchingComplexLineInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBlockFlow& blockFlow, const Container& inlineFormattingRoot)
{
    auto& inlineFormattingState = layoutState.establishedFormattingState(inlineFormattingRoot);
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    auto& inlineRunList = downcast<InlineFormattingState>(inlineFormattingState).inlineRuns();

    // Collect inlineboxes.
    Vector<WebCore::InlineBox*> inlineBoxes;
    collectInlineBoxes(blockFlow, inlineBoxes);

    auto mismatched = false;
    unsigned runIndex = 0;

    if (inlineBoxes.size() != inlineRunList.size()) {
        stream << "Warning: mismatching number of runs: inlineboxes(" << inlineBoxes.size() << ") vs. inline runs(" << inlineRunList.size() << ")";
        stream.nextLine();
    }

    for (unsigned inlineBoxIndex = 0; inlineBoxIndex < inlineBoxes.size() && runIndex < inlineRunList.size(); ++inlineBoxIndex) {
        auto* inlineBox = inlineBoxes[inlineBoxIndex];
        auto* inlineTextBox = is<InlineTextBox>(inlineBox) ? downcast<InlineTextBox>(inlineBox) : nullptr;

        auto& inlineRun = inlineRunList[runIndex];
        auto matchingRuns = false;
        if (inlineTextBox) {
            auto xOffset = resolveForRelativePositionIfNeeded(*inlineTextBox);
            matchingRuns = checkForMatchingTextRuns(inlineRun, inlineTextBox->logicalLeft() + xOffset,
                inlineTextBox->logicalRight() + xOffset,
                inlineTextBox->start(),
                inlineTextBox->end() + 1,
                inlineTextBox->logicalHeight());

            // <span>foobar</span>foobar generates 2 inline text boxes while we only generate one inline run.
            // also <div>foo<img style="float: left;">bar</div> too.
            auto inlineRunEnd = inlineRun.textContext()->start() + inlineRun.textContext()->length();
            auto textRunMightBeExtended = !matchingRuns && inlineTextBox->end() < inlineRunEnd && inlineBoxIndex < inlineBoxes.size() - 1;

            if (textRunMightBeExtended) {
                auto logicalLeft = inlineTextBox->logicalLeft() + xOffset;
                auto logicalRight = inlineTextBox->logicalRight() + xOffset;
                auto start = inlineTextBox->start();
                auto end = inlineTextBox->end() + 1;
                auto index = ++inlineBoxIndex;
                for (; index < inlineBoxes.size(); ++index) {
                    auto* inlineBox = inlineBoxes[index];
                    auto* inlineTextBox = is<InlineTextBox>(inlineBox) ? downcast<InlineTextBox>(inlineBox) : nullptr;
                    // Can't mix different inline boxes.
                    if (!inlineTextBox)
                        break;

                    auto xOffset = resolveForRelativePositionIfNeeded(*inlineTextBox);
                    logicalRight = inlineTextBox->logicalRight() + xOffset;
                    end += (inlineTextBox->end() + 1);
                    if (checkForMatchingTextRuns(inlineRun, logicalLeft, logicalRight, start, end, inlineTextBox->logicalHeight())) {
                        matchingRuns = true;
                        inlineBoxIndex = index;
                        break;
                    }

                    // Went too far?
                    if (end >= inlineRunEnd)
                        break;
                }
            }
        } else
            matchingRuns = checkForMatchingNonTextRuns(inlineRun, *inlineBox);


        if (!matchingRuns) {
            stream << "Mismatching: run ";

            if (inlineTextBox)
                stream << "(" << inlineTextBox->start() << ", " << inlineTextBox->end() + 1 << ")";
            stream << " (" << inlineBox->logicalLeft() << ", " << inlineBox->logicalRight() << ") (" << inlineBox->logicalWidth() << "x" << inlineBox->logicalHeight() << ")";

            stream << "inline run ";
            if (inlineRun.textContext())
                stream << "(" << inlineRun.textContext()->start() << ", " << inlineRun.textContext()->start() + inlineRun.textContext()->length() << ") ";
            stream << "(" << inlineRun.logicalLeft() << ", " << inlineRun.logicalRight() << ") (" << inlineRun.logicalWidth() << "x" << inlineRun.logicalHeight() << ")";
            stream.nextLine();
            mismatched = true;
        }
        ++runIndex;
    }
    return mismatched;
}

static bool outputMismatchingBlockBoxInformationIfNeeded(TextStream& stream, const LayoutState& context, const RenderBox& renderer, const Box& layoutBox)
{
    bool firstMismatchingRect = true;
    auto outputRect = [&] (const String& prefix, const LayoutRect& rendererRect, const LayoutRect& layoutRect) {
        if (firstMismatchingRect) {
            stream << (renderer.element() ? renderer.element()->nodeName().utf8().data() : "") << " " << renderer.renderName() << "(" << &renderer << ") layoutBox(" << &layoutBox << ")";
            stream.nextLine();
            firstMismatchingRect = false;
        }

        stream  << prefix.utf8().data() << "\trenderer->(" << rendererRect.x() << "," << rendererRect.y() << ") (" << rendererRect.width() << "x" << rendererRect.height() << ")"
            << "\tlayout->(" << layoutRect.x() << "," << layoutRect.y() << ") (" << layoutRect.width() << "x" << layoutRect.height() << ")"; 
        stream.nextLine();
    };

    auto renderBoxLikeMarginBox = [](auto& displayBox) {
        // Produce a RenderBox matching margin box.
        auto borderBox = displayBox.borderBox();

        return Display::Box::Rect {
            borderBox.top() - displayBox.nonCollapsedMarginBefore(),
            borderBox.left() - displayBox.computedMarginStart().valueOr(0),
            displayBox.computedMarginStart().valueOr(0) + borderBox.width() + displayBox.computedMarginEnd().valueOr(0),
            displayBox.nonCollapsedMarginBefore() + borderBox.height() + displayBox.nonCollapsedMarginAfter()
        };
    };

    auto& displayBox = context.displayBoxForLayoutBox(layoutBox);

    auto frameRect = renderer.frameRect();
    // rendering does not offset for relative positioned boxes.
    if (renderer.isInFlowPositioned())
        frameRect.move(renderer.offsetForInFlowPosition());

    if (frameRect != displayBox.rect()) {
        outputRect("frameBox", renderer.frameRect(), displayBox.rect());
        return true;
    }

    if (renderer.borderBoxRect() != displayBox.borderBox()) {
        outputRect("borderBox", renderer.borderBoxRect(), displayBox.borderBox());
        return true;
    }

    if (renderer.paddingBoxRect() != displayBox.paddingBox()) {
        outputRect("paddingBox", renderer.paddingBoxRect(), displayBox.paddingBox());
        return true;
    }

    if (renderer.contentBoxRect() != displayBox.contentBox()) {
        outputRect("contentBox", renderer.contentBoxRect(), displayBox.contentBox());
        return true;
    }

    if (renderer.marginBoxRect() != renderBoxLikeMarginBox(displayBox)) {
        // In certain cases, like out-of-flow boxes with margin auto, marginBoxRect() returns 0. It's clearly incorrect,
        // so let's check the individual margin values instead (and at this point we know that all other boxes match).
        auto marginsMatch = displayBox.marginBefore() == renderer.marginBefore()
            && displayBox.marginAfter() == renderer.marginAfter()
            && displayBox.marginStart() == renderer.marginStart()
            && displayBox.marginEnd() == renderer.marginEnd();

        if (!marginsMatch) {
            outputRect("marginBox", renderer.marginBoxRect(), renderBoxLikeMarginBox(displayBox));
            return true;
        }
    }

    return false;
}

static bool verifyAndOutputSubtree(TextStream& stream, const LayoutState& context, const RenderBox& renderer, const Box& layoutBox)
{
    auto mismtachingGeometry = outputMismatchingBlockBoxInformationIfNeeded(stream, context, renderer, layoutBox);

    if (!is<Container>(layoutBox))
        return mismtachingGeometry;

    auto& container = downcast<Container>(layoutBox);
    auto* childBox = container.firstChild();
    auto* childRenderer = renderer.firstChild();

    while (childRenderer) {
        if (!is<RenderBox>(*childRenderer)) {
            childRenderer = childRenderer->nextSibling();
            continue;
        }

        if (!childBox) {
            stream  << "Trees are out of sync!";
            stream.nextLine();
            return true;
        }

        if (is<RenderBlockFlow>(*childRenderer) && childBox->establishesInlineFormattingContext()) {
            ASSERT(childRenderer->childrenInline());
            auto& blockFlow = downcast<RenderBlockFlow>(*childRenderer);
            auto& formattingRoot = downcast<Container>(*childBox);
            mismtachingGeometry |= blockFlow.lineLayoutPath() == RenderBlockFlow::SimpleLinesPath ? outputMismatchingSimpleLineInformationIfNeeded(stream, context, blockFlow, formattingRoot) : outputMismatchingComplexLineInformationIfNeeded(stream, context, blockFlow, formattingRoot);
        } else {
            auto mismatchingSubtreeGeometry = verifyAndOutputSubtree(stream, context, downcast<RenderBox>(*childRenderer), *childBox);
            mismtachingGeometry |= mismatchingSubtreeGeometry;
        }

        childBox = childBox->nextSibling();
        childRenderer = childRenderer->nextSibling();
    }

    return mismtachingGeometry;
}

void LayoutState::verifyAndOutputMismatchingLayoutTree(const RenderView& renderView) const
{
    TextStream stream;
    auto mismatchingGeometry = verifyAndOutputSubtree(stream, *this, renderView, initialContainingBlock());
    if (!mismatchingGeometry)
        return;
#if ENABLE(TREE_DEBUGGING)
    showRenderTree(&renderView);
    showLayoutTree(initialContainingBlock(), this);
#endif
    WTFLogAlways("%s", stream.release().utf8().data());
    ASSERT_NOT_REACHED();
}

}
}

#endif
