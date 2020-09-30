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

#include "DisplayContainerBox.h"
#include "DisplayImageBox.h"
#include "DisplayStyle.h"
#include "DisplayTextBox.h"
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
    : m_pixelSnappingFactor(pixelSnappingFactor)
{
}

std::unique_ptr<Tree> TreeBuilder::build(const Layout::LayoutState& layoutState) const
{
    ASSERT(layoutState.hasRoot());

    auto& rootLayoutBox = layoutState.root();

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Building display tree for:");
    showLayoutTree(rootLayoutBox, &layoutState);
#endif

    auto geometry = layoutState.geometryForBox(rootLayoutBox);
    auto rootDisplayBox = displayBoxForRootBox(geometry, rootLayoutBox);
    auto rootDisplayContainerBox = std::unique_ptr<ContainerBox> { downcast<ContainerBox>(rootDisplayBox.release()) };

    if (!rootLayoutBox.firstChild())
        return makeUnique<Tree>(WTFMove(rootDisplayContainerBox));

    auto borderBox = LayoutRect { geometry.logicalRect() };
    auto offset = toLayoutSize(borderBox.location());
    recursiveBuildDisplayTree(layoutState, offset, *rootLayoutBox.firstChild(), *rootDisplayContainerBox);

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Display tree:");
    showDisplayTree(*rootDisplayContainerBox);
#endif

    return makeUnique<Tree>(WTFMove(rootDisplayContainerBox));
}

void TreeBuilder::buildInlineDisplayTree(const Layout::LayoutState& layoutState, LayoutSize offsetFromRoot, const Layout::ContainerBox& inlineFormattingRoot, ContainerBox& parentDisplayBox) const
{
    auto& inlineFormattingState = layoutState.establishedInlineFormattingState(inlineFormattingRoot);

    Box* previousSiblingBox = nullptr;

    for (auto& run : inlineFormattingState.lineRuns()) {
        auto& lineGeometry = inlineFormattingState.lines().at(run.lineIndex());

        auto lineRect = lineGeometry.logicalRect();
        auto lineLayoutRect = LayoutRect { lineRect.left(), lineRect.top(), lineRect.width(), lineRect.height() };

        auto runRect = LayoutRect { run.logicalLeft(), run.logicalTop(), run.logicalWidth(), run.logicalHeight() };
        runRect.moveBy(lineLayoutRect.location());
        runRect.move(offsetFromRoot);

        auto style = Style { run.layoutBox().style() };
        auto textBox = makeUnique<TextBox>(snapRectToDevicePixels(runRect, m_pixelSnappingFactor), WTFMove(style), run);
        auto textBoxPtr = textBox.get();

        if (previousSiblingBox)
            previousSiblingBox->setNextSibling(WTFMove(textBox));
        else
            parentDisplayBox.setFirstChild(WTFMove(textBox));

        previousSiblingBox = textBoxPtr;
    }
}

Box* TreeBuilder::recursiveBuildDisplayTree(const Layout::LayoutState& layoutState, LayoutSize offsetFromRoot, const Layout::Box& box, ContainerBox& parentDisplayBox, Box* previousSiblingBox) const
{
    auto geometry = layoutState.geometryForBox(box);
    auto displayBox = displayBoxForLayoutBox(geometry, box, offsetFromRoot);
    
    Box* result = displayBox.get();

    if (previousSiblingBox)
        previousSiblingBox->setNextSibling(WTFMove(displayBox));
    else
        parentDisplayBox.setFirstChild(WTFMove(displayBox));

    if (!is<Layout::ContainerBox>(box))
        return result;

    auto& layoutContainerBox = downcast<Layout::ContainerBox>(box);
    if (!layoutContainerBox.hasChild())
        return result;

    auto borderBox = LayoutRect { geometry.logicalRect() };
    offsetFromRoot += toLayoutSize(borderBox.location());

    auto& displayContainerBox = downcast<ContainerBox>(*result);

    if (layoutContainerBox.establishesInlineFormattingContext()) {
        buildInlineDisplayTree(layoutState, offsetFromRoot, downcast<Layout::ContainerBox>(layoutContainerBox), displayContainerBox);
        return result;
    }

    Display::Box* currSiblingDisplayBox = nullptr;
    for (auto& child : Layout::childrenOfType<Layout::Box>(layoutContainerBox)) {
        if (layoutState.hasBoxGeometry(child))
            currSiblingDisplayBox = recursiveBuildDisplayTree(layoutState, offsetFromRoot, child, displayContainerBox, currSiblingDisplayBox);
    }

    return result;
}

std::unique_ptr<Box> TreeBuilder::displayBoxForRootBox(const Layout::BoxGeometry& geometry, const Layout::ContainerBox& rootBox) const
{
    // FIXME: Need to do logical -> physical coordinate mapping here.
    auto borderBox = LayoutRect { geometry.logicalRect() };

    auto style = Style { rootBox.style() };
    return makeUnique<ContainerBox>(snapRectToDevicePixels(borderBox, m_pixelSnappingFactor), WTFMove(style));
}

std::unique_ptr<Box> TreeBuilder::displayBoxForLayoutBox(const Layout::BoxGeometry& geometry, const Layout::Box& layoutBox, LayoutSize offsetFromRoot) const
{
    // FIXME: Need to map logical to physical rects.
    auto borderBox = LayoutRect { geometry.logicalRect() };
    borderBox.move(offsetFromRoot);
    auto pixelSnappedBorderBox = snapRectToDevicePixels(borderBox, m_pixelSnappingFactor);

    // FIXME: Handle isAnonymous()
    // FIXME: Do hoisting of <body> styles to the root where appropriate.

    // FIXME: Need to do logical -> physical coordinate mapping here.
    auto style = Style { layoutBox.style() };
    
    if (is<Layout::ReplacedBox>(layoutBox)) {
        // FIXME: Wrong; geometry needs to vend the correct replaced content rect.
        auto replacedContentRect = LayoutRect { geometry.contentBoxLeft(), geometry.contentBoxTop(), geometry.contentBoxWidth(), geometry.contentBoxHeight() };
        replacedContentRect.moveBy(borderBox.location());
        auto pixelSnappedReplacedContentRect = snapRectToDevicePixels(replacedContentRect, m_pixelSnappingFactor);

        // FIXME: Don't assume it's an image.
        auto imageBox = makeUnique<ImageBox>(pixelSnappedBorderBox, WTFMove(style), pixelSnappedReplacedContentRect);

        if (auto* cachedImage = downcast<Layout::ReplacedBox>(layoutBox).cachedImage())
            imageBox->setImage(cachedImage->image());

        return imageBox;
    }
    
    if (is<Layout::ContainerBox>(layoutBox)) {
        // FIXME: The decision to make a ContainerBox should be made based on whether this Display::Box will have children.
        return makeUnique<ContainerBox>(pixelSnappedBorderBox, WTFMove(style));
    }

    return makeUnique<Box>(snapRectToDevicePixels(borderBox, m_pixelSnappingFactor), WTFMove(style));
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

void showDisplayTree(const Box& box)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputDisplayTree(stream, box, 1);
    WTFLogAlways("%s", stream.release().utf8().data());
}

#endif

} // namespace Display
} // namespace WebCore

#endif // ENABLE(LAYOUT_FORMATTING_CONTEXT)
