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

#ifndef NDEBUG
#include "BlockFormattingState.h"
#include "InlineFormattingState.h"
#include "InlineTextBox.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContainerBox.h"
#include "LayoutContext.h"
#include "LayoutTreeBuilder.h"
#include "RenderBox.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderTableCell.h"
#include "RenderTableSection.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static bool areEssentiallyEqual(LayoutUnit a, LayoutUnit b)
{
    if (a == b)
        return true;
    // 1/4th CSS pixel.
    constexpr float epsilon = kFixedPointDenominator / 4;
    return abs(a.rawValue() - b.rawValue()) <= epsilon;
}

static bool areEssentiallyEqual(float a, InlineLayoutUnit b)
{
    return areEssentiallyEqual(LayoutUnit { a }, LayoutUnit { b });
}

static bool areEssentiallyEqual(LayoutRect a, LayoutRect b)
{
    return areEssentiallyEqual(a.x(), b.x())
        && areEssentiallyEqual(a.y(), b.y())
        && areEssentiallyEqual(a.width(), b.width())
        && areEssentiallyEqual(a.height(), b.height());
}

static bool outputMismatchingSimpleLineInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBlockFlow& blockFlow, const ContainerBox& inlineFormattingRoot)
{
    auto* lineLayoutData = blockFlow.simpleLineLayout();
    if (!lineLayoutData) {
        ASSERT_NOT_REACHED();
        return true;
    }

    auto& inlineFormattingState = layoutState.establishedFormattingState(inlineFormattingRoot);
    auto* displayInlineContent = downcast<InlineFormattingState>(inlineFormattingState).displayInlineContent();
    if (!displayInlineContent) {
        ASSERT_NOT_REACHED();
        return true;
    }

    auto& displayRuns = displayInlineContent->runs;

    if (displayRuns.size() != lineLayoutData->runCount()) {
        stream << "Mismatching number of runs: simple runs(" << lineLayoutData->runCount() << ") inline runs(" << displayRuns.size() << ")";
        stream.nextLine();
        return true;
    }

    auto mismatched = false;
    for (unsigned i = 0; i < lineLayoutData->runCount(); ++i) {
        auto& simpleRun = lineLayoutData->runAt(i);
        auto& displayRun = displayRuns[i];

        auto matchingRuns = areEssentiallyEqual(simpleRun.logicalLeft, displayRun.left()) && areEssentiallyEqual(simpleRun.logicalRight, displayRun.right());
        if (matchingRuns && displayRun.textContent()) {
            matchingRuns = simpleRun.start == displayRun.textContent()->start() && simpleRun.end == displayRun.textContent()->end();
            // SLL handles strings in a more concatenated format <div>foo<br>bar</div> -> foo -> 0,3 bar -> 3,6 vs. 0,3 and 0,3
            if (!matchingRuns)
                matchingRuns = (simpleRun.end - simpleRun.start) == (displayRun.textContent()->end() - displayRun.textContent()->start());
        }
        if (matchingRuns)
            continue;

        stream << "Mismatching: simple run(" << simpleRun.start << ", " << simpleRun.end << ") (" << simpleRun.logicalLeft << ", " << simpleRun.logicalRight << ")";
        stream << " inline run";
        if (displayRun.textContent())
            stream << " (" << displayRun.textContent()->start() << ", " << displayRun.textContent()->end() << ")";
        stream << " (" << displayRun.left() << ", " << displayRun.top() << ") (" << displayRun.width() << "x" << displayRun.height() << ")";
        stream.nextLine();
        mismatched = true;
    }
    return mismatched;
}

static bool checkForMatchingNonTextRuns(const Display::Run& inlineRun, const WebCore::InlineBox& inlineBox)
{
    return areEssentiallyEqual(inlineBox.left(), inlineRun.left())
        && areEssentiallyEqual(inlineBox.right(), inlineRun.right())
        && areEssentiallyEqual(inlineBox.top(), inlineRun.top())
        && areEssentiallyEqual(inlineBox.bottom(), inlineRun.bottom());
}


static bool checkForMatchingTextRuns(const Display::Run& inlineRun, const WebCore::InlineTextBox& inlineTextBox)
{
    return areEssentiallyEqual(inlineTextBox.left(), inlineRun.left())
        && areEssentiallyEqual(inlineTextBox.right(), inlineRun.right())
        && areEssentiallyEqual(inlineTextBox.top(), inlineRun.top())
        && areEssentiallyEqual(inlineTextBox.bottom(), inlineRun.bottom())
        && (inlineTextBox.isLineBreak() || (inlineTextBox.start() == inlineRun.textContent()->start() && inlineTextBox.end() == inlineRun.textContent()->end()));
}

static void collectFlowBoxSubtree(const InlineFlowBox& flowbox, Vector<WebCore::InlineBox*>& inlineBoxes)
{
    auto* inlineBox = flowbox.firstLeafDescendant();
    auto* lastLeafDescendant = flowbox.lastLeafDescendant();
    while (inlineBox) {
        inlineBoxes.append(inlineBox);
        if (inlineBox == lastLeafDescendant)
            break;
        inlineBox = inlineBox->nextLeafOnLine();
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

static bool outputMismatchingComplexLineInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBlockFlow& blockFlow, const ContainerBox& inlineFormattingRoot)
{
    auto& inlineFormattingState = layoutState.establishedFormattingState(inlineFormattingRoot);

    auto* displayInlineContent = downcast<InlineFormattingState>(inlineFormattingState).displayInlineContent();
    if (!displayInlineContent) {
        ASSERT_NOT_REACHED();
        return true;
    }
    auto& displayRuns = displayInlineContent->runs;

    // Collect inlineboxes.
    Vector<WebCore::InlineBox*> inlineBoxes;
    collectInlineBoxes(blockFlow, inlineBoxes);

    auto mismatched = false;
    unsigned runIndex = 0;

    if (inlineBoxes.size() != displayRuns.size()) {
        stream << "Warning: mismatching number of runs: inlineboxes(" << inlineBoxes.size() << ") vs. inline runs(" << displayRuns.size() << ")";
        stream.nextLine();
    }

    for (unsigned inlineBoxIndex = 0; inlineBoxIndex < inlineBoxes.size() && runIndex < displayRuns.size(); ++inlineBoxIndex) {
        auto& displayRun = displayRuns[runIndex];
        auto* inlineBox = inlineBoxes[inlineBoxIndex];
        auto* inlineTextBox = is<InlineTextBox>(inlineBox) ? downcast<InlineTextBox>(inlineBox) : nullptr;
        bool matchingRuns = inlineTextBox ? checkForMatchingTextRuns(displayRun, *inlineTextBox) : checkForMatchingNonTextRuns(displayRun, *inlineBox);

        if (!matchingRuns) {
            
            if (is<RenderLineBreak>(inlineBox->renderer())) {
                // <br> positioning is weird at this point. It needs proper baseline.
                matchingRuns = true;
                ++runIndex;
                continue;
            }

            stream << "Mismatching: run";

            if (inlineTextBox)
                stream << " (" << inlineTextBox->start() << ", " << inlineTextBox->end() << ")";
            stream << " (" << inlineBox->logicalLeft() << ", " << inlineBox->logicalTop() << ") (" << inlineBox->logicalWidth() << "x" << inlineBox->logicalHeight() << ")";

            stream << " inline run";
            if (displayRun.textContent())
                stream << " (" << displayRun.textContent()->start() << ", " << displayRun.textContent()->end() << ")";
            stream << " (" << displayRun.left() << ", " << displayRun.top() << ") (" << displayRun.width() << "x" << displayRun.height() << ")";
            stream.nextLine();
            mismatched = true;
        }
        ++runIndex;
    }
    return mismatched;
}

static bool outputMismatchingBlockBoxInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBox& renderer, const Box& layoutBox)
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

    auto renderBoxLikeMarginBox = [&] (const auto& boxGeometry) {
        if (layoutBox.isInitialContainingBlock())
            return boxGeometry.logicalRect();

        // Produce a RenderBox matching margin box.
        auto containingBlockWidth = layoutState.geometryForBox(layoutBox.containingBlock()).contentBoxWidth();
        auto marginStart = LayoutUnit { };
        auto& marginStartStyle = layoutBox.style().marginStart();
        if (marginStartStyle.isFixed() || marginStartStyle.isPercent() || marginStartStyle.isCalculated())
            marginStart = valueForLength(marginStartStyle, containingBlockWidth);

        auto marginEnd = LayoutUnit { };
        auto& marginEndStyle = layoutBox.style().marginEnd();
        if (marginEndStyle.isFixed() || marginEndStyle.isPercent() || marginEndStyle.isCalculated())
            marginEnd = valueForLength(marginEndStyle, containingBlockWidth);

        auto marginBefore = boxGeometry.marginBefore();
        auto marginAfter = boxGeometry.marginAfter();
        if (layoutBox.formattingContextRoot().establishesBlockFormattingContext()) {
            auto& formattingState = downcast<BlockFormattingState>(layoutState.formattingStateForBox(layoutBox));
            auto verticalMargin = formattingState.usedVerticalMargin(layoutBox);
            marginBefore = verticalMargin.nonCollapsedValues.before;
            marginAfter = verticalMargin.nonCollapsedValues.after;
        }
        auto borderBox = boxGeometry.borderBox();
        return Rect {
            borderBox.top() - marginBefore,
            borderBox.left() - marginStart,
            marginStart + borderBox.width() + marginEnd,
            marginBefore + borderBox.height() + marginAfter
        };
    };

    // rendering does not offset for relative positioned boxes.
    auto frameRect = renderer.frameRect();
    if (renderer.isInFlowPositioned())
        frameRect.move(renderer.offsetForInFlowPosition());

    auto boxGeometry = BoxGeometry { layoutState.geometryForBox(layoutBox) };
    if (layoutBox.isTableBox()) {
        // When the <table> is out-of-flow positioned, the wrapper table box has the offset
        // while the actual table box is static, inflow.
        auto& tableWrapperBoxGeometry = layoutState.geometryForBox(layoutBox.containingBlock());
        boxGeometry.moveBy(tableWrapperBoxGeometry.logicalTopLeft());
        // Table wrapper box has the margin values for the table.
        boxGeometry.setHorizontalMargin(tableWrapperBoxGeometry.horizontalMargin());
        boxGeometry.setVerticalMargin(tableWrapperBoxGeometry.verticalMargin());
    }

    if (is<RenderTableRow>(renderer) || is<RenderTableSection>(renderer)) {
        // Table rows and tbody have 0 width for some reason when border collapsing is on.
        if (is<RenderTableRow>(renderer) && downcast<RenderTableRow>(renderer).table()->collapseBorders())
            return false;
        // Section borders are either collapsed or ignored. However they may produce negative padding boxes.
        if (is<RenderTableSection>(renderer) && (downcast<RenderTableSection>(renderer).table()->collapseBorders() || renderer.style().hasBorder()))
            return false;
    }
    if (!areEssentiallyEqual(frameRect, boxGeometry.logicalRect())) {
        outputRect("frameBox", renderer.frameRect(), boxGeometry.logicalRect());
        return true;
    }

    if (!areEssentiallyEqual(renderer.borderBoxRect(), boxGeometry.borderBox())) {
        outputRect("borderBox", renderer.borderBoxRect(), boxGeometry.borderBox());
        return true;
    }

    // When the table row border overflows the row, padding box becomes negative and content box is incorrect.
    auto shouldCheckPaddingAndContentBox = !is<RenderTableRow>(renderer) || renderer.paddingBoxRect().width() >= 0;
    if (shouldCheckPaddingAndContentBox && !areEssentiallyEqual(renderer.paddingBoxRect(), boxGeometry.paddingBox())) {
        outputRect("paddingBox", renderer.paddingBoxRect(), boxGeometry.paddingBox());
        return true;
    }

    auto shouldCheckContentBox = [&] {
        if (!shouldCheckPaddingAndContentBox)
            return false;
        // FIXME: Figure out why trunk/rendering comes back with odd values for <tbody> and <td> content box.
        if (is<RenderTableCell>(renderer) || is<RenderTableSection>(renderer))
            return false;
        // Tables have 0 content box size for some reason when border collapsing is on.
        return !is<RenderTable>(renderer) || !downcast<RenderTable>(renderer).collapseBorders();
    }();
    if (shouldCheckContentBox && !areEssentiallyEqual(renderer.contentBoxRect(), boxGeometry.contentBox())) {
        outputRect("contentBox", renderer.contentBoxRect(), boxGeometry.contentBox());
        return true;
    }

    if (!areEssentiallyEqual(renderer.marginBoxRect(), renderBoxLikeMarginBox(boxGeometry))) {
        // In certain cases, like out-of-flow boxes with margin auto, marginBoxRect() returns 0. It's clearly incorrect,
        // so let's check the individual margin values instead (and at this point we know that all other boxes match).
        auto marginsMatch = boxGeometry.marginBefore() == renderer.marginBefore()
            && boxGeometry.marginAfter() == renderer.marginAfter()
            && boxGeometry.marginStart() == renderer.marginStart()
            && boxGeometry.marginEnd() == renderer.marginEnd();

        if (!marginsMatch) {
            outputRect("marginBox", renderer.marginBoxRect(), renderBoxLikeMarginBox(boxGeometry));
            return true;
        }
    }

    return false;
}

static bool verifyAndOutputSubtree(TextStream& stream, const LayoutState& context, const RenderBox& renderer, const Box& layoutBox)
{
    // Rendering code does not have the concept of table wrapper box. Skip it by verifying the first child(table box) instead. 
    if (layoutBox.isTableWrapperBox())
        return verifyAndOutputSubtree(stream, context, renderer, *downcast<ContainerBox>(layoutBox).firstChild()); 

    auto mismtachingGeometry = outputMismatchingBlockBoxInformationIfNeeded(stream, context, renderer, layoutBox);

    if (!is<ContainerBox>(layoutBox))
        return mismtachingGeometry;

    auto& containerBox = downcast<ContainerBox>(layoutBox);
    auto* childLayoutBox = containerBox.firstChild();
    auto* childRenderer = renderer.firstChild();

    while (childRenderer) {
        if (!is<RenderBox>(*childRenderer)) {
            childRenderer = childRenderer->nextSibling();
            continue;
        }

        if (!childLayoutBox) {
            stream  << "Trees are out of sync!";
            stream.nextLine();
            return true;
        }

        if (is<RenderBlockFlow>(*childRenderer) && childLayoutBox->establishesInlineFormattingContext()) {
            ASSERT(childRenderer->childrenInline());
            auto mismtachingGeometry = outputMismatchingBlockBoxInformationIfNeeded(stream, context, downcast<RenderBox>(*childRenderer), *childLayoutBox);
            if (mismtachingGeometry)
                return true;

            auto& blockFlow = downcast<RenderBlockFlow>(*childRenderer);
            auto& formattingRoot = downcast<ContainerBox>(*childLayoutBox);
            mismtachingGeometry |= blockFlow.lineLayoutPath() == RenderBlockFlow::SimpleLinesPath ? outputMismatchingSimpleLineInformationIfNeeded(stream, context, blockFlow, formattingRoot) : outputMismatchingComplexLineInformationIfNeeded(stream, context, blockFlow, formattingRoot);
        } else {
            auto mismatchingSubtreeGeometry = verifyAndOutputSubtree(stream, context, downcast<RenderBox>(*childRenderer), *childLayoutBox);
            mismtachingGeometry |= mismatchingSubtreeGeometry;
        }

        childLayoutBox = childLayoutBox->nextSibling();
        childRenderer = childRenderer->nextSibling();
    }

    return mismtachingGeometry;
}

void LayoutContext::verifyAndOutputMismatchingLayoutTree(const LayoutState& layoutState, const RenderView& rootRenderer)
{
    TextStream stream;
    auto& layoutRoot = layoutState.root();
    auto mismatchingGeometry = verifyAndOutputSubtree(stream, layoutState, rootRenderer, layoutRoot);
    if (!mismatchingGeometry)
        return;
#if ENABLE(TREE_DEBUGGING)
    showRenderTree(&rootRenderer);
    showLayoutTree(layoutRoot, &layoutState);
#endif
    WTFLogAlways("%s", stream.release().utf8().data());
    ASSERT_NOT_REACHED();
}

}
}

#endif

#endif
