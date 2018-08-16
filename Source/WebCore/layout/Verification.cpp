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
#include "LayoutContext.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBox.h"
#include "InlineTextBox.h"
#include "LayoutBox.h"
#include "LayoutContainer.h"
#include "LayoutTreeBuilder.h"
#include "RenderBox.h"
#include "RenderView.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Layout {

static bool outputMismatchingSimpleLineInformationIfNeeded(TextStream& stream, const LayoutContext& layoutContext, const RenderBlockFlow& blockFlow, const Container& inlineFormattingRoot)
{
    auto* lineLayoutData = blockFlow.simpleLineLayout();
    if (!lineLayoutData) {
        ASSERT_NOT_REACHED();
        return true;
    }

    auto& inlineFormattingState = const_cast<LayoutContext&>(layoutContext).establishedFormattingState(inlineFormattingRoot);
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    auto& layoutRuns = downcast<InlineFormattingState>(inlineFormattingState).layoutRuns();

    if (layoutRuns.size() != lineLayoutData->runCount()) {
        stream << "Mismatching number of runs: simple runs(" << lineLayoutData->runCount() << ") layout runs(" << layoutRuns.size() << ")";
        stream.nextLine();
        return true;
    }

    auto mismatched = false;
    for (unsigned i = 0; i < lineLayoutData->runCount(); ++i) {
        auto& simpleRun = lineLayoutData->runAt(i);
        auto& layoutRun = layoutRuns[i];

        if (simpleRun.start == layoutRun.start() && simpleRun.end == layoutRun.end() && simpleRun.logicalLeft == layoutRun.left() && simpleRun.logicalRight == layoutRun.right())
            continue;

        stream << "Mismatching: simple run(" << simpleRun.start << ", " << simpleRun.end << ") (" << simpleRun.logicalLeft << ", " << simpleRun.logicalRight << ") layout run(" << layoutRun.start() << ", " << layoutRun.end() << ") (" << layoutRun.left() << ", " << layoutRun.right() << ")";
        stream.nextLine();
        mismatched = true;
    }
    return mismatched;
}

static bool outputMismatchingComplexLineInformationIfNeeded(TextStream& stream, const LayoutContext& layoutContext, const RenderBlockFlow& blockFlow, const Container& inlineFormattingRoot)
{
    auto& inlineFormattingState = const_cast<LayoutContext&>(layoutContext).establishedFormattingState(inlineFormattingRoot);
    ASSERT(is<InlineFormattingState>(inlineFormattingState));
    auto& layoutRuns = downcast<InlineFormattingState>(inlineFormattingState).layoutRuns();

    auto mismatched = false;
    unsigned layoutRunIndex = 0;
    for (auto* rootLine = blockFlow.firstRootBox(); rootLine; rootLine = rootLine->nextRootBox()) {
        for (auto* lineBox = rootLine->firstChild(); lineBox; lineBox = lineBox->nextOnLine()) {
            if (!is<InlineTextBox>(lineBox))
                continue;

            if (layoutRunIndex >= layoutRuns.size()) {
                // FIXME: <span>foobar</span>foobar generates 2 inline text boxes while we only generate one layout run (which is much better, since it enables us to do kerning across inline elements).
                stream << "Mismatching number of runs: layout runs(" << layoutRuns.size() << ")";
                stream.nextLine();
                return true;
            }

            auto& layoutRun = layoutRuns[layoutRunIndex];
            auto& inlineTextBox = downcast<InlineTextBox>(*lineBox);
            if (inlineTextBox.start() == layoutRun.start() && inlineTextBox.end() == layoutRun.end() && inlineTextBox.logicalLeft() == layoutRun.left() && inlineTextBox.logicalRight() == layoutRun.right()) {
                stream << "Mismatching: simple run(" << inlineTextBox.start() << ", " << inlineTextBox.end() << ") (" << inlineTextBox.logicalLeft() << ", " << inlineTextBox.logicalRight() << ") layout run(" << layoutRun.start() << ", " << layoutRun.end() << ") (" << layoutRun.left() << ", " << layoutRun.right() << ")";
                stream.nextLine();
                mismatched = true;
            }
            ++layoutRunIndex;
        }
    }
    return mismatched;
}

static bool outputMismatchingBlockBoxInformationIfNeeded(TextStream& stream, const LayoutContext& context, const RenderBox& renderer, const Box& layoutBox)
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

    auto* displayBox = context.displayBoxForLayoutBox(layoutBox);
    ASSERT(displayBox);

    auto frameRect = renderer.frameRect();
    // rendering does not offset for relative positioned boxes.
    if (renderer.isInFlowPositioned())
        frameRect.move(renderer.offsetForInFlowPosition());

    if (frameRect != displayBox->rect()) {
        outputRect("frameBox", renderer.frameRect(), displayBox->rect());
        return true;
    }

    if (renderer.borderBoxRect() != displayBox->borderBox()) {
        outputRect("borderBox", renderer.borderBoxRect(), displayBox->borderBox());
        return true;
    }

    if (renderer.paddingBoxRect() != displayBox->paddingBox()) {
        outputRect("paddingBox", renderer.paddingBoxRect(), displayBox->paddingBox());
        return true;
    }

    if (renderer.contentBoxRect() != displayBox->contentBox()) {
        outputRect("contentBox", renderer.contentBoxRect(), displayBox->contentBox());
        return true;
    }

    // TODO: The RenderBox::marginBox() does not follow the spec and ignores certain constraints. Skip them for now.
    return false;
}

static bool verifyAndOutputSubtree(TextStream& stream, const LayoutContext& context, const RenderBox& renderer, const Box& layoutBox)
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

void LayoutContext::verifyAndOutputMismatchingLayoutTree(const RenderView& renderView) const
{
    TextStream stream;
    auto mismatchingGeometry = verifyAndOutputSubtree(stream, *this, renderView, *m_root.get());
    if (!mismatchingGeometry)
        return;
#if ENABLE(TREE_DEBUGGING)
    showRenderTree(&renderView);
    showLayoutTree(*m_root.get(), this);
#endif
    WTFLogAlways("%s", stream.release().utf8().data());
}

}
}

#endif
