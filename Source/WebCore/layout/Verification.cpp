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

#ifndef NDEBUG
#include "BlockFormattingState.h"
#include "InlineFormattingState.h"
#include "LayoutBox.h"
#include "LayoutBoxGeometry.h"
#include "LayoutContext.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
#include "LayoutTreeBuilder.h"
#include "LegacyInlineTextBox.h"
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

static bool checkForMatchingNonTextRuns(const InlineDisplay::Box& box, const WebCore::LegacyInlineBox& inlineBox)
{
    return areEssentiallyEqual(inlineBox.left(), box.left())
        && areEssentiallyEqual(inlineBox.right(), box.right())
        && areEssentiallyEqual(inlineBox.top(), box.top())
        && areEssentiallyEqual(inlineBox.bottom(), box.bottom());
}


static bool checkForMatchingTextRuns(InlineDisplay::Box& box, const WebCore::LegacyInlineTextBox& inlineTextBox)
{
    if (!box.text())
        return false;
    return areEssentiallyEqual(inlineTextBox.left(), box.left())
        && areEssentiallyEqual(inlineTextBox.right(), box.right())
        && areEssentiallyEqual(inlineTextBox.top(), box.top())
        && areEssentiallyEqual(inlineTextBox.bottom(), box.bottom())
        && (inlineTextBox.isLineBreak() || (inlineTextBox.start() == box.text()->start() && inlineTextBox.end() == box.text()->end()));
}

static void collectFlowBoxSubtree(const LegacyInlineFlowBox& flowbox, Vector<WebCore::LegacyInlineBox*>& inlineBoxes)
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

static void collectInlineBoxes(const RenderBlockFlow& root, Vector<WebCore::LegacyInlineBox*>& inlineBoxes)
{
    for (auto* rootLine = root.firstRootBox(); rootLine; rootLine = rootLine->nextRootBox()) {
        for (auto* inlineBox = rootLine->firstChild(); inlineBox; inlineBox = inlineBox->nextOnLine()) {
            if (!is<LegacyInlineFlowBox>(inlineBox)) {
                inlineBoxes.append(inlineBox);
                continue;
            }
            collectFlowBoxSubtree(downcast<LegacyInlineFlowBox>(*inlineBox), inlineBoxes);
        }
    }
}

static bool outputMismatchingComplexLineInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBlockFlow& blockFlow, const ElementBox& inlineFormattingRoot)
{
    auto& inlineFormattingState = layoutState.formattingStateForFormattingContext(inlineFormattingRoot);
    auto& boxes = downcast<InlineFormattingState>(inlineFormattingState).boxes();
    // Collect inlineboxes.
    Vector<WebCore::LegacyInlineBox*> inlineBoxes;
    collectInlineBoxes(blockFlow, inlineBoxes);

    auto mismatched = false;
    unsigned boxIndex = 0;

    if (inlineBoxes.size() != boxes.size()) {
        stream << "Warning: mismatching number of boxes: inlineboxes(" << inlineBoxes.size() << ") vs. inline boxes(" << boxes.size() << ")";
        stream.nextLine();
    }

    for (unsigned inlineBoxIndex = 0; inlineBoxIndex < inlineBoxes.size() && boxIndex < boxes.size(); ++inlineBoxIndex) {
        auto& box = boxes[boxIndex];
        auto* inlineBox = inlineBoxes[inlineBoxIndex];
        auto* inlineTextBox = dynamicDowncast<WebCore::LegacyInlineTextBox>(inlineBox);
        bool matchingRuns = inlineTextBox ? checkForMatchingTextRuns(box, *inlineTextBox) : checkForMatchingNonTextRuns(box, *inlineBox);

        if (!matchingRuns) {
            
            if (is<RenderLineBreak>(inlineBox->renderer())) {
                // <br> positioning is weird at this point. It needs proper baseline.
                matchingRuns = true;
                ++boxIndex;
                continue;
            }

            stream << "Mismatching: box";

            if (inlineTextBox)
                stream << " (" << inlineTextBox->start() << ", " << inlineTextBox->end() << ")";
            stream << " (" << inlineBox->logicalLeft() << ", " << inlineBox->logicalTop() << ") (" << inlineBox->logicalWidth() << "x" << inlineBox->logicalHeight() << ")";

            stream << " inline box";
            if (box.text())
                stream << " (" << box.text()->start() << ", " << box.text()->end() << ")";
            stream << " (" << box.left() << ", " << box.top() << ") (" << box.width() << "x" << box.height() << ")";
            stream.nextLine();
            mismatched = true;
        }
        ++boxIndex;
    }
    return mismatched;
}

static bool outputMismatchingBlockBoxInformationIfNeeded(TextStream& stream, const LayoutState& layoutState, const RenderBox& renderer, const Box& layoutBox)
{
    bool firstMismatchingRect = true;
    auto outputRect = [&] (ASCIILiteral prefix, const LayoutRect& rendererRect, const LayoutRect& layoutRect) {
        if (firstMismatchingRect) {
            stream << (renderer.element() ? renderer.element()->nodeName().utf8().data() : "") << " " << renderer.renderName().characters() << "(" << &renderer << ") layoutBox(" << &layoutBox << ")";
            stream.nextLine();
            firstMismatchingRect = false;
        }

        stream  << prefix.characters() << "\trenderer->(" << rendererRect.x() << "," << rendererRect.y() << ") (" << rendererRect.width() << "x" << rendererRect.height() << ")"
            << "\tlayout->(" << layoutRect.x() << "," << layoutRect.y() << ") (" << layoutRect.width() << "x" << layoutRect.height() << ")"; 
        stream.nextLine();
    };

    auto renderBoxLikeMarginBox = [&] (const auto& boxGeometry) {
        if (layoutBox.isInitialContainingBlock())
            return BoxGeometry::borderBoxRect(boxGeometry);

        // Produce a RenderBox matching margin box.
        auto containingBlockWidth = layoutState.geometryForBox(FormattingContext::containingBlock(layoutBox)).contentBoxWidth();
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
        if (FormattingContext::formattingContextRoot(layoutBox).establishesBlockFormattingContext()) {
            auto& formattingState = downcast<BlockFormattingState>(layoutState.formattingStateForFormattingContext(FormattingContext::formattingContextRoot(layoutBox)));
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
        auto& tableWrapperBoxGeometry = layoutState.geometryForBox(FormattingContext::containingBlock(layoutBox));
        boxGeometry.moveBy(BoxGeometry::borderBoxTopLeft(tableWrapperBoxGeometry));
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
    if (!areEssentiallyEqual(frameRect, BoxGeometry::borderBoxRect(boxGeometry))) {
        outputRect("frameBox"_s, renderer.frameRect(), BoxGeometry::borderBoxRect(boxGeometry));
        return true;
    }

    if (!areEssentiallyEqual(renderer.borderBoxRect(), boxGeometry.borderBox())) {
        outputRect("borderBox"_s, renderer.borderBoxRect(), boxGeometry.borderBox());
        return true;
    }

    // When the table row border overflows the row, padding box becomes negative and content box is incorrect.
    auto shouldCheckPaddingAndContentBox = !is<RenderTableRow>(renderer) || renderer.paddingBoxRect().width() >= 0;
    if (shouldCheckPaddingAndContentBox && !areEssentiallyEqual(renderer.paddingBoxRect(), boxGeometry.paddingBox())) {
        outputRect("paddingBox"_s, renderer.paddingBoxRect(), boxGeometry.paddingBox());
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
        outputRect("contentBox"_s, renderer.contentBoxRect(), boxGeometry.contentBox());
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
            outputRect("marginBox"_s, renderer.marginBoxRect(), renderBoxLikeMarginBox(boxGeometry));
            return true;
        }
    }

    return false;
}

static bool verifyAndOutputSubtree(TextStream& stream, const LayoutState& context, const RenderBox& renderer, const Box& layoutBox)
{
    // Rendering code does not have the concept of table wrapper box. Skip it by verifying the first child(table box) instead. 
    if (layoutBox.isTableWrapperBox())
        return verifyAndOutputSubtree(stream, context, renderer, *downcast<ElementBox>(layoutBox).firstChild()); 

    auto mismtachingGeometry = outputMismatchingBlockBoxInformationIfNeeded(stream, context, renderer, layoutBox);

    if (!is<ElementBox>(layoutBox))
        return mismtachingGeometry;

    auto& elementBox = downcast<ElementBox>(layoutBox);
    auto* childLayoutBox = elementBox.firstChild();
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
            auto& formattingRoot = downcast<ElementBox>(*childLayoutBox);
            mismtachingGeometry |= outputMismatchingComplexLineInformationIfNeeded(stream, context, blockFlow, formattingRoot);
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
    showLayoutTree(downcast<InitialContainingBlock>(layoutRoot), &layoutState);
#endif
    WTFLogAlways("%s", stream.release().utf8().data());
    ASSERT_NOT_REACHED();
}

}
}

#endif

