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

class PositioningContext {
public:
    PositioningContext(const Display::ContainerBox& rootDisplayBox)
        : m_fixedPositionContainer({ rootDisplayBox, { } })
        , m_absolutePositionContainer({ rootDisplayBox, { } })
        , m_inFlowContainer ({ rootDisplayBox, { } })
    {
    }

    PositioningContext contextForDescendants(const Layout::Box& layoutBox, const Layout::BoxGeometry geometry, const Display::ContainerBox& displayBox) const
    {
        auto currentOffset = containingBlockContextForLayoutBox(layoutBox).offsetFromRoot;

        auto borderBoxRect = LayoutRect { Layout::BoxGeometry::borderBoxRect(geometry) };
        currentOffset += toLayoutSize(borderBoxRect.location());

        auto currentBoxes = ContainingBlockContext { displayBox, currentOffset };
        return {
            layoutBox.isContainingBlockForFixedPosition() ? currentBoxes : m_fixedPositionContainer,
            layoutBox.isContainingBlockForOutOfFlowPosition() ? currentBoxes : m_absolutePositionContainer,
            layoutBox.isContainingBlockForInFlow() ? currentBoxes : m_inFlowContainer
        };
    }

    const ContainingBlockContext& containingBlockContextForLayoutBox(const Layout::Box& layoutBox) const
    {
        if (layoutBox.isFixedPositioned())
            return m_fixedPositionContainer;

        if (layoutBox.isOutOfFlowPositioned())
            return m_absolutePositionContainer;

        return m_inFlowContainer;
    }
    
    const ContainingBlockContext& inFlowContainingBlockContext() const { return m_inFlowContainer; }

private:
    PositioningContext(const ContainingBlockContext& fixedContainer, const ContainingBlockContext& absoluteContainer, const ContainingBlockContext& inFlowContainer)
        : m_fixedPositionContainer(fixedContainer)
        , m_absolutePositionContainer(absoluteContainer)
        , m_inFlowContainer(inFlowContainer)
    {
    }

    ContainingBlockContext m_fixedPositionContainer;
    ContainingBlockContext m_absolutePositionContainer;
    ContainingBlockContext m_inFlowContainer;
};

struct BuildingState {
    PositioningContext positioningContext;
};

TreeBuilder::TreeBuilder(float pixelSnappingFactor)
    : m_boxFactory(pixelSnappingFactor)
    , m_stateStack(makeUnique<Vector<BuildingState>>())
{
    m_stateStack->reserveInitialCapacity(32);
}

TreeBuilder::~TreeBuilder() = default;

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

    auto insertionPosition = InsertionPosition { *rootDisplayContainerBox };

    m_stateStack->append({ *rootDisplayContainerBox });

    recursiveBuildDisplayTree(layoutState, *rootLayoutBox.firstChild(), insertionPosition);

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Display tree:\n" << displayTreeAsText(*rootDisplayContainerBox));
#endif

    return makeUnique<Tree>(WTFMove(rootDisplayContainerBox));
}

void TreeBuilder::pushStateForBoxDescendants(const Layout::ContainerBox& layoutContainerBox, const Layout::BoxGeometry& layoutGeometry, const ContainerBox& displayBox)
{
    auto& positioningContext = m_stateStack->last().positioningContext;
    m_stateStack->append({ positioningContext.contextForDescendants(layoutContainerBox, layoutGeometry, displayBox) });
}

void TreeBuilder::popState()
{
    m_stateStack->removeLast();
}

const BuildingState& TreeBuilder::currentState() const
{
    ASSERT(m_stateStack && m_stateStack->size());
    return m_stateStack->last();
}

const PositioningContext& TreeBuilder::positioningContext() const
{
    return currentState().positioningContext;
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

void TreeBuilder::buildInlineDisplayTree(const Layout::LayoutState& layoutState, const Layout::ContainerBox& inlineFormattingRoot, InsertionPosition& insertionPosition)
{
    auto& inlineFormattingState = layoutState.establishedInlineFormattingState(inlineFormattingRoot);

    for (auto& run : inlineFormattingState.lineRuns()) {
        if (run.text()) {
            auto& lineGeometry = inlineFormattingState.lines().at(run.lineIndex());
            auto textBox = m_boxFactory.displayBoxForTextRun(run, lineGeometry, positioningContext().inFlowContainingBlockContext());
            insert(WTFMove(textBox), insertionPosition);
            continue;
        }

        if (is<Layout::ContainerBox>(run.layoutBox())) {
            recursiveBuildDisplayTree(layoutState, run.layoutBox(), insertionPosition);
            continue;
        }

        auto geometry = layoutState.geometryForBox(run.layoutBox());
        auto displayBox = m_boxFactory.displayBoxForLayoutBox(run.layoutBox(), geometry, positioningContext().inFlowContainingBlockContext());
        insert(WTFMove(displayBox), insertionPosition);
    }
}

void TreeBuilder::recursiveBuildDisplayTree(const Layout::LayoutState& layoutState, const Layout::Box& layoutBox, InsertionPosition& insertionPosition)
{
    auto geometry = layoutState.geometryForBox(layoutBox);
    std::unique_ptr<Box> displayBox;

    auto& containingBlockContext = positioningContext().containingBlockContextForLayoutBox(layoutBox);
    if (layoutBox.isBodyBox())
        displayBox = m_boxFactory.displayBoxForBodyBox(layoutBox, geometry, containingBlockContext, m_rootBackgroundPropgation);
    else
        displayBox = m_boxFactory.displayBoxForLayoutBox(layoutBox, geometry, containingBlockContext);
    
    insert(WTFMove(displayBox), insertionPosition);

    if (!is<Layout::ContainerBox>(layoutBox))
        return;

    auto& layoutContainerBox = downcast<Layout::ContainerBox>(layoutBox);
    if (!layoutContainerBox.hasChild())
        return;

    ContainerBox& currentBox = downcast<ContainerBox>(*insertionPosition.currentChild);
    auto insertionPositionForChildren = InsertionPosition { currentBox };

    pushStateForBoxDescendants(layoutContainerBox, geometry, currentBox);

    enum class DescendantBoxInclusion { AllBoxes, OutOfFlowOnly };
    auto boxInclusion = DescendantBoxInclusion::AllBoxes;

    if (layoutContainerBox.establishesInlineFormattingContext()) {
        buildInlineDisplayTree(layoutState, downcast<Layout::ContainerBox>(layoutContainerBox), insertionPositionForChildren);
        boxInclusion = DescendantBoxInclusion::OutOfFlowOnly;
    }

    auto includeBox = [](DescendantBoxInclusion boxInclusion, const Layout::Box& box) {
        switch (boxInclusion) {
        case DescendantBoxInclusion::AllBoxes: return true;
        case DescendantBoxInclusion::OutOfFlowOnly: return !box.isInFlow();
        }
        return false;
    };

    for (auto& child : Layout::childrenOfType<Layout::Box>(layoutContainerBox)) {
        if (includeBox(boxInclusion, child) && layoutState.hasBoxGeometry(child))
            recursiveBuildDisplayTree(layoutState, child, insertionPositionForChildren);
    }

    popState();
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
