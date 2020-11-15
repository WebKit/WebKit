/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "DisplayTreeBuilder.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "DisplayBoxFactory.h"
#include "DisplayContainerBox.h"
#include "DisplayStyle.h"
#include "DisplayTree.h"
#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutContainerBox.h"
#include "LayoutReplacedBox.h"
#include "LayoutState.h"
#include "LayoutTreeBuilder.h" // Just for showLayoutTree.
#include "Logging.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Display {

TreeBuilder::TreeBuilder(float pixelSnappingFactor)
    : m_boxFactory(pixelSnappingFactor)
{
}

std::unique_ptr<Tree> TreeBuilder::build(const Layout::LayoutState& layoutState)
{
    ASSERT(layoutState.hasRoot());

    auto& rootLayoutBox = layoutState.root();

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Building display tree for:\n" << layoutTreeAsText(rootLayoutBox, &layoutState));
#endif

    m_rootBackgroundPropgation = BoxFactory::determineRootBackgroundPropagation(rootLayoutBox);

    auto geometry = layoutState.geometryForBox(rootLayoutBox);
    auto rootDisplayBox = m_boxFactory.displayBoxForRootBox(rootLayoutBox, geometry, m_rootBackgroundPropgation);
    auto rootDisplayContainerBox = std::unique_ptr<ContainerBox> { downcast<ContainerBox>(rootDisplayBox.release()) };

    if (!rootLayoutBox.firstChild())
        return makeUnique<Tree>(WTFMove(rootDisplayContainerBox));

    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };
    auto offset = toLayoutSize(borderBoxRect.location());
    auto insertionPosition = InsertionPosition { *rootDisplayContainerBox };

    recursiveBuildDisplayTree(layoutState, offset, *rootLayoutBox.firstChild(), insertionPosition);

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Display tree:\n" << displayTreeAsText(*rootDisplayContainerBox));
#endif

    return makeUnique<Tree>(WTFMove(rootDisplayContainerBox));
}

void TreeBuilder::insert(std::unique_ptr<Box>&& box, InsertionPosition& insertionPosition) const
{
    if (insertionPosition.currentChild) {
        auto boxPtr = box.get();
        insertionPosition.currentChild->setNextSibling(WTFMove(box));
        insertionPosition.currentChild = boxPtr;
    } else {
        insertionPosition.currentChild = box.get();
        insertionPosition.container.setFirstChild(WTFMove(box));
    }
}

void TreeBuilder::buildInlineDisplayTree(const Layout::LayoutState& layoutState, LayoutSize offsetFromRoot, const Layout::ContainerBox& inlineFormattingRoot, InsertionPosition& insertionPosition) const
{
    auto& inlineFormattingState = layoutState.establishedInlineFormattingState(inlineFormattingRoot);

    for (auto& run : inlineFormattingState.lineRuns()) {
        if (run.text()) {
            auto& lineGeometry = inlineFormattingState.lines().at(run.lineIndex());
            auto textBox = m_boxFactory.displayBoxForTextRun(run, lineGeometry, offsetFromRoot);
            insert(WTFMove(textBox), insertionPosition);
            continue;
        }

        if (is<Layout::ContainerBox>(run.layoutBox())) {
            recursiveBuildDisplayTree(layoutState, offsetFromRoot, run.layoutBox(), insertionPosition);
            continue;
        }

        auto geometry = layoutState.geometryForBox(run.layoutBox());
        auto displayBox = m_boxFactory.displayBoxForLayoutBox(run.layoutBox(), geometry, offsetFromRoot);
        insert(WTFMove(displayBox), insertionPosition);
    }
}

void TreeBuilder::recursiveBuildDisplayTree(const Layout::LayoutState& layoutState, LayoutSize offsetFromRoot, const Layout::Box& layoutBox, InsertionPosition& insertionPosition) const
{
    auto geometry = layoutState.geometryForBox(layoutBox);
    std::unique_ptr<Box> displayBox;
    
    if (layoutBox.isBodyBox())
        displayBox = m_boxFactory.displayBoxForBodyBox(layoutBox, geometry, m_rootBackgroundPropgation, offsetFromRoot);
    else
        displayBox = m_boxFactory.displayBoxForLayoutBox(layoutBox, geometry, offsetFromRoot);
    
    insert(WTFMove(displayBox), insertionPosition);

    if (!is<Layout::ContainerBox>(layoutBox))
        return;

    auto& layoutContainerBox = downcast<Layout::ContainerBox>(layoutBox);
    if (!layoutContainerBox.hasChild())
        return;

    auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };
    offsetFromRoot += toLayoutSize(borderBoxRect.location());

    auto positionForChildren = InsertionPosition { downcast<ContainerBox>(*insertionPosition.currentChild) };
    
    if (layoutContainerBox.establishesInlineFormattingContext()) {
        buildInlineDisplayTree(layoutState, offsetFromRoot, downcast<Layout::ContainerBox>(layoutContainerBox), positionForChildren);
        return;
    }

    for (auto& child : Layout::childrenOfType<Layout::Box>(layoutContainerBox)) {
        if (layoutState.hasBoxGeometry(child))
            recursiveBuildDisplayTree(layoutState, offsetFromRoot, child, positionForChildren);
    }
}

#if ENABLE(TREE_DEBUGGING)

static void outputDisplayBox(TextStream& stream, const Box& displayBox, unsigned)
{
    stream.writeIndent();

    stream << displayBox.debugDescription();
    stream << "\n";
}

static void outputDisplayTree(TextStream& stream, const Box& displayBox, unsigned depth)
{
    outputDisplayBox(stream, displayBox, depth);

    if (is<ContainerBox>(displayBox)) {
        TextStream::IndentScope indent(stream);
        for (auto child = downcast<ContainerBox>(displayBox).firstChild(); child; child = child->nextSibling())
            outputDisplayTree(stream, *child, depth + 1);
    }
}

String displayTreeAsText(const Box& box)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputDisplayTree(stream, box, 1);
    return stream.release();
}

void showDisplayTree(const Box& box)
{
    auto treeAsText = displayTreeAsText(box);
    WTFLogAlways("%s", treeAsText.utf8().data());
}

#endif

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
