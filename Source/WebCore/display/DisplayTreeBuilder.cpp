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

#include "DisplayBoxFactory.h"
#include "DisplayContainerBox.h"
#include "DisplayStackingItem.h"
#include "DisplayStyle.h"
#include "DisplayTree.h"
#include "InlineFormattingState.h"
#include "LayoutBoxGeometry.h"
#include "LayoutChildIterator.h"
#include "LayoutElementBox.h"
#include "LayoutInitialContainingBlock.h"
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
    const ContainerBox& inFlowContainingBlockBox() const { return positioningContext.inFlowContainingBlockContext().box; }

    PositioningContext positioningContext;
    StackingItem& currentStackingItem;
    StackingItem& currentStackingContextItem;

    UnadjustedAbsoluteFloatRect currentStackingItemPaintedContentExtent; // This stacking item's contents only.
    UnadjustedAbsoluteFloatRect currentStackingItemPaintingExtent; // Including descendant items.
};

TreeBuilder::TreeBuilder(float pixelSnappingFactor)
    : m_boxFactory(*this, pixelSnappingFactor)
    , m_stateStack(makeUnique<Vector<BuildingState>>())
{
    m_stateStack->reserveInitialCapacity(32);
}

TreeBuilder::~TreeBuilder() = default;

std::unique_ptr<Tree> TreeBuilder::build(const Layout::LayoutState& layoutState)
{
    auto& rootLayoutBox = layoutState.root();

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Building display tree for:\n" << layoutTreeAsText(downcast<Layout::InitialContainingBlock>(rootLayoutBox), &layoutState));
#endif

    ASSERT(!m_tree);
    m_tree = makeUnique<Tree>();

    m_rootBackgroundPropgation = BoxFactory::determineRootBackgroundPropagation(rootLayoutBox);

    auto geometry = layoutState.geometryForBox(rootLayoutBox);
    auto rootDisplayBox = m_boxFactory.displayBoxForRootBox(rootLayoutBox, geometry, m_rootBackgroundPropgation);

    auto insertionPosition = InsertionPosition { *rootDisplayBox };
    auto rootStackingItem = makeUnique<StackingItem>(WTFMove(rootDisplayBox));

    if (rootLayoutBox.firstChild()) {
        auto& rootContainerBox = downcast<ContainerBox>(rootStackingItem->box());
        m_stateStack->append({ { rootContainerBox }, *rootStackingItem, *rootStackingItem, { }, { } });
        recursiveBuildDisplayTree(layoutState, *rootLayoutBox.firstChild(), insertionPosition);
    }

#if ENABLE(TREE_DEBUGGING)
    LOG_WITH_STREAM(FormattingContextLayout, stream << "Display tree:\n" << displayTreeAsText(*rootStackingItem));
#endif
    m_tree->setRootStackingItem(WTFMove(rootStackingItem));
    return WTFMove(m_tree);
}

void TreeBuilder::pushStateForBoxDescendants(const Layout::ElementBox& layoutContainerBox, const Layout::BoxGeometry& layoutGeometry, const ContainerBox& displayBox, StackingItem* boxStackingItem)
{
    auto& positioningContext = m_stateStack->last().positioningContext;

    auto currentStackingItem = [&]() -> StackingItem& {
        if (boxStackingItem)
            return *boxStackingItem;

        return m_stateStack->last().currentStackingItem;
    };

    auto currentStackingContextItem = [&]() -> StackingItem& {
        if (boxStackingItem && boxStackingItem->isStackingContext())
            return *boxStackingItem;
        
        return m_stateStack->last().currentStackingContextItem;
    };

    m_stateStack->append({
        positioningContext.contextForDescendants(layoutContainerBox, layoutGeometry, displayBox),
        currentStackingItem(),
        currentStackingContextItem(),
        { },
        { }
    });
}

void TreeBuilder::popState(const BoxModelBox& currentBox)
{
    ASSERT(m_stateStack->size() > 1);
    auto& currentState = m_stateStack->last();
    auto& previousState = m_stateStack->at(m_stateStack->size() - 2);

    bool endingStackingItem = &currentState.currentStackingItem.box() == &currentBox;
    bool endingStackingContextItem = &currentState.currentStackingContextItem.box() == &currentBox;

    // We've accumulated into currentStackingItemPaintedContentExtent while building descendant boxes.
    auto paintedContentExtent = currentState.currentStackingItemPaintedContentExtent;

    // Often, but not always, containingBlockBox will be currentBox. If it has overflow clipping, clip the rect from descendants.
    auto& containingBlockBox = currentState.inFlowContainingBlockBox();
    if (containingBlockBox.style().hasClippedOverflow()) {
        auto clipRect = containingBlockBox.absolutePaddingBoxRect();
        paintedContentExtent.intersect(clipRect);
    }

    // Now union with this box's extent (which is outside the clip).
    auto boxPaintingExtent = unionRect(paintedContentExtent, currentBox.absolutePaintingExtent());

    if (endingStackingItem) {
        currentState.currentStackingItem.setPaintedContentBounds(boxPaintingExtent);
        
        // Now accumulate this item's extent (possibly clipped) into that of its parent item.
        auto descendantItemsPaintingExtent = currentState.currentStackingItemPaintingExtent;

        if (containingBlockBox.style().hasClippedOverflow()) {
            auto clipRect = containingBlockBox.absolutePaddingBoxRect();
            descendantItemsPaintingExtent.intersect(clipRect);
        }

        auto itemPaintingExtent = unionRect(descendantItemsPaintingExtent, boxPaintingExtent);
        currentState.currentStackingItem.setPaintedBoundsIncludingDescendantItems(itemPaintingExtent);

        previousState.currentStackingItemPaintingExtent.unite(itemPaintingExtent);
    } else
        previousState.currentStackingItemPaintedContentExtent.unite(boxPaintingExtent);

    if (endingStackingContextItem)
        currentState.currentStackingContextItem.sortLists();

    m_stateStack->removeLast();
}

void TreeBuilder::didAppendNonContainerStackingItem(StackingItem& item)
{
    // This is s simplified version of pushStateForBoxDescendants/popState.
    auto currentStackingContextItem = [&]() -> StackingItem& {
        if (item.isStackingContext())
            return item;
        
        return m_stateStack->last().currentStackingContextItem;
    };

    m_stateStack->append({
        m_stateStack->last().positioningContext,
        item,
        currentStackingContextItem(),
        { },
        { }
    });

    popState(item.box());
}

void TreeBuilder::accountForBoxPaintingExtent(const Box& box)
{
    // FIXME: Need to account for transforms.
    currentState().currentStackingItemPaintedContentExtent.unite(box.absolutePaintingExtent());
}

BuildingState& TreeBuilder::currentState()
{
    ASSERT(m_stateStack && m_stateStack->size());
    return m_stateStack->last();
}

const PositioningContext& TreeBuilder::positioningContext()
{
    return currentState().positioningContext;
}

void TreeBuilder::insert(std::unique_ptr<Box>&& box, InsertionPosition& insertionPosition) const
{
    box->setParent(&insertionPosition.container);
    if (insertionPosition.currentChild) {
        auto boxPtr = box.get();
        insertionPosition.currentChild->setNextSibling(WTFMove(box));
        insertionPosition.currentChild = boxPtr;
    } else {
        insertionPosition.currentChild = box.get();
        insertionPosition.container.setFirstChild(WTFMove(box));
    }
}

StackingItem* TreeBuilder::insertIntoTree(std::unique_ptr<Box>&& box, InsertionPosition& insertionPosition, WillTraverseDescendants willTraverseDescendants)
{
    if (box->participatesInZOrderSorting() && is<BoxModelBox>(*box)) {
        auto boxModelBox = std::unique_ptr<BoxModelBox> { downcast<BoxModelBox>(box.release()) };
        auto stackingItem = makeUnique<StackingItem>(WTFMove(boxModelBox));

        // This painted bounds will get extended as we traverse box descendants.
        stackingItem->setPaintedContentBounds(stackingItem->box().absolutePaintingExtent());

        auto* stackingItemPtr = stackingItem.get();
        currentState().currentStackingContextItem.addChildStackingItem(WTFMove(stackingItem));
        
        if (willTraverseDescendants == WillTraverseDescendants::No)
            didAppendNonContainerStackingItem(*stackingItemPtr);

        return stackingItemPtr;
    }

    insert(WTFMove(box), insertionPosition);

    if (willTraverseDescendants == WillTraverseDescendants::No)
        accountForBoxPaintingExtent(*insertionPosition.currentChild);

    return nullptr;
}

void TreeBuilder::buildInlineDisplayTree(const Layout::LayoutState& layoutState, const Layout::ElementBox& inlineFormattingRoot, InsertionPosition& insertionPosition)
{
    auto& inlineFormattingState = layoutState.formattingStateForInlineFormattingContext(inlineFormattingRoot);

    for (auto& box : inlineFormattingState.boxes()) {
        if (box.isRootInlineBox()) {
            // Not supported yet.
            continue;
        }

        if (box.text()) {
            auto& lineGeometry = inlineFormattingState.lines().at(box.lineIndex());
            auto textBox = m_boxFactory.displayBoxForTextRun(box, lineGeometry, positioningContext().inFlowContainingBlockContext());
            insert(WTFMove(textBox), insertionPosition);
            accountForBoxPaintingExtent(*insertionPosition.currentChild);
            continue;
        }

        if (is<Layout::ElementBox>(box.layoutBox())) {
            recursiveBuildDisplayTree(layoutState, box.layoutBox(), insertionPosition);
            continue;
        }

        // FIXME: Workaround for webkit.orgb/b/219335.
        if (box.layoutBox().isLineBreakBox() && box.layoutBox().isOutOfFlowPositioned())
            continue;

        auto geometry = layoutState.geometryForBox(box.layoutBox());
        auto displayBox = m_boxFactory.displayBoxForLayoutBox(box.layoutBox(), geometry, positioningContext().inFlowContainingBlockContext());
        insertIntoTree(WTFMove(displayBox), insertionPosition, WillTraverseDescendants::No);
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

    Box& currentBox = *displayBox;

    auto willTraverseDescendants = (is<Layout::ElementBox>(layoutBox) && downcast<Layout::ElementBox>(layoutBox).hasChild()) ? WillTraverseDescendants::Yes : WillTraverseDescendants::No;
    auto* stackingItem = insertIntoTree(WTFMove(displayBox), insertionPosition, willTraverseDescendants);
    if (willTraverseDescendants == WillTraverseDescendants::No)
        return;

    auto& layoutContainerBox = downcast<Layout::ElementBox>(layoutBox);

    ContainerBox& currentContainerBox = downcast<ContainerBox>(currentBox);
    pushStateForBoxDescendants(layoutContainerBox, geometry, currentContainerBox, stackingItem);

    auto insertionPositionForChildren = InsertionPosition { currentContainerBox };

    enum class DescendantBoxInclusion { AllBoxes, OutOfFlowOnly };
    auto boxInclusion = DescendantBoxInclusion::AllBoxes;

    if (layoutContainerBox.establishesInlineFormattingContext()) {
        buildInlineDisplayTree(layoutState, downcast<Layout::ElementBox>(layoutContainerBox), insertionPositionForChildren);
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

    popState(currentContainerBox);
}

#if ENABLE(TREE_DEBUGGING)

static void outputDisplayBox(TextStream& stream, const Box& displayBox, bool writeIndent = true)
{
    if (writeIndent)
        stream.writeIndent();

    stream << displayBox.debugDescription();
    stream << "\n";
}

static void outputDisplayTree(TextStream& stream, const Box& displayBox, bool writeFirstItemIndent = true)
{
    outputDisplayBox(stream, displayBox, writeFirstItemIndent);

    if (is<ContainerBox>(displayBox)) {
        TextStream::IndentScope indent(stream);
        for (auto child = downcast<ContainerBox>(displayBox).firstChild(); child; child = child->nextSibling())
            outputDisplayTree(stream, *child);
    }
}

String displayTreeAsText(const Box& box)
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    outputDisplayTree(stream, box);
    return stream.release();
}

void showDisplayTree(const Box& box)
{
    auto treeAsText = displayTreeAsText(box);
    WTFLogAlways("%s", treeAsText.utf8().data());
}

static void outputStackingTree(TextStream& stream, const char* prefix, const StackingItem& stackingItem)
{
    stream.writeIndent();
    stream << "[" << prefix << "] item (" << &stackingItem << ") " << stackingItem.paintedContentBounds() << " - ";

    TextStream::IndentScope indent(stream);
    outputDisplayTree(stream, stackingItem.box(), false);

    {
        TextStream::IndentScope indent(stream);
        for (auto& childStackingItem : stackingItem.negativeZOrderList())
            outputStackingTree(stream, "-", *childStackingItem);

        for (auto& childStackingItem : stackingItem.positiveZOrderList())
            outputStackingTree(stream, childStackingItem->isStackingContext() ? "+" : "p", *childStackingItem);
    }
}

String displayTreeAsText(const StackingItem& stackingItem)
{
    TextStream stream(TextStream::LineMode::MultipleLine);
    outputStackingTree(stream, "+", stackingItem);
    return stream.release();
}

void showDisplayTree(const StackingItem& stackingItem)
{
    auto treeAsText = displayTreeAsText(stackingItem);
    WTFLogAlways("%s", treeAsText.utf8().data());
}

#endif

} // namespace Display
} // namespace WebCore

