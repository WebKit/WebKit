/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003-2015, 2017 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "RenderTreeBuilderMultiColumn.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderTreeBuilder.h"
#include "RenderTreeBuilderBlock.h"
#include "RenderView.h"

namespace WebCore {

static RenderMultiColumnSet* findSetRendering(const RenderMultiColumnFlow& fragmentedFlow, const RenderObject& renderer)
{
    // Find the set inside which the specified renderer would be rendered.
    for (auto* multicolSet = fragmentedFlow.firstMultiColumnSet(); multicolSet; multicolSet = multicolSet->nextSiblingMultiColumnSet()) {
        if (multicolSet->containsRendererInFragmentedFlow(renderer))
            return multicolSet;
    }
    return nullptr;
}

static RenderObject* spannerPlacehoderCandidate(const RenderObject& renderer, const RenderMultiColumnFlow& stayWithin)
{
    // Spanner candidate is a next sibling/ancestor's next child within the flow thread and
    // it is in the same inflow/out-of-flow layout context.
    if (renderer.isOutOfFlowPositioned())
        return nullptr;

    ASSERT(renderer.isDescendantOf(&stayWithin));
    auto* current = &renderer;
    while (true) {
        // Skip to the first in-flow sibling.
        auto* nextSibling = current->nextSibling();
        while (nextSibling && nextSibling->isOutOfFlowPositioned())
            nextSibling = nextSibling->nextSibling();
        if (nextSibling)
            return nextSibling;
        // No sibling candidate, jump to the parent and check its siblings.
        current = current->parent();
        if (!current || current == &stayWithin || current->isOutOfFlowPositioned())
            return nullptr;
    }
    return nullptr;
}

static bool isValidColumnSpanner(const RenderMultiColumnFlow& fragmentedFlow, const RenderObject& descendant)
{
    // We assume that we're inside the flow thread. This function is not to be called otherwise.
    ASSERT(descendant.isDescendantOf(&fragmentedFlow));
    // First make sure that the renderer itself has the right properties for becoming a spanner.
    if (!is<RenderBox>(descendant))
        return false;

    auto& descendantBox = downcast<RenderBox>(descendant);
    if (descendantBox.isFloatingOrOutOfFlowPositioned())
        return false;

    if (descendantBox.style().columnSpan() != ColumnSpan::All)
        return false;

    auto* parent = descendantBox.parent();
    if (!is<RenderBlockFlow>(*parent) || parent->childrenInline()) {
        // Needs to be block-level.
        return false;
    }

    // We need to have the flow thread as the containing block. A spanner cannot break out of the flow thread.
    auto* enclosingFragmentedFlow = descendantBox.enclosingFragmentedFlow();
    if (enclosingFragmentedFlow != &fragmentedFlow)
        return false;

    // This looks like a spanner, but if we're inside something unbreakable, it's not to be treated as one.
    for (auto* ancestor = descendantBox.containingBlock(); ancestor; ancestor = ancestor->containingBlock()) {
        if (is<RenderView>(*ancestor))
            return false;
        if (ancestor->isLegend())
            return false;
        if (is<RenderFragmentedFlow>(*ancestor)) {
            // Don't allow any intervening non-multicol fragmentation contexts. The spec doesn't say
            // anything about disallowing this, but it's just going to be too complicated to
            // implement (not to mention specify behavior).
            return ancestor == &fragmentedFlow;
        }
        if (is<RenderBlockFlow>(*ancestor)) {
            auto& blockFlowAncestor = downcast<RenderBlockFlow>(*ancestor);
            if (blockFlowAncestor.willCreateColumns()) {
                // This ancestor (descendent of the fragmentedFlow) will create columns later. The spanner belongs to it.
                return false;
            }
            if (blockFlowAncestor.multiColumnFlow()) {
                // While this ancestor (descendent of the fragmentedFlow) has a fragmented flow context, this context is being destroyed.
                // However the spanner still belongs to it (will most likely be moved to the parent fragmented context as the next step).
                return false;
            }
        }
        ASSERT(ancestor->style().columnSpan() != ColumnSpan::All || !isValidColumnSpanner(fragmentedFlow, *ancestor));
        if (ancestor->isUnsplittableForPagination())
            return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

RenderTreeBuilder::MultiColumn::MultiColumn(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::MultiColumn::updateAfterDescendants(RenderBlockFlow& flow)
{
    bool needsFragmentedFlow = flow.requiresColumns(flow.style().columnCount());
    bool hasFragmentedFlow = flow.multiColumnFlow();

    if (!hasFragmentedFlow && needsFragmentedFlow) {
        createFragmentedFlow(flow);
        return;
    }
    if (hasFragmentedFlow && !needsFragmentedFlow) {
        destroyFragmentedFlow(flow);
        return;
    }
}

void RenderTreeBuilder::MultiColumn::createFragmentedFlow(RenderBlockFlow& flow)
{
    flow.setChildrenInline(false); // Do this to avoid wrapping inline children that are just going to move into the flow thread.
    flow.deleteLines();
    // If this soon-to-be multicolumn flow is already part of a multicolumn context, we need to move back the descendant spanners
    // to their original position before moving subtrees around.
    auto* enclosingflow = flow.enclosingFragmentedFlow();
    if (is<RenderMultiColumnFlow>(enclosingflow)) {
        auto& spanners = downcast<RenderMultiColumnFlow>(enclosingflow)->spannerMap();
        Vector<RenderMultiColumnSpannerPlaceholder*> placeholdersToDelete;
        for (auto& spannerAndPlaceholder : spanners) {
            auto& placeholder = *spannerAndPlaceholder.value;
            if (!placeholder.isDescendantOf(&flow))
                continue;
            placeholdersToDelete.append(&placeholder);
        }
        for (auto* placeholder : placeholdersToDelete) {
            auto* spanner = placeholder->spanner();
            if (!spanner) {
                ASSERT_NOT_REACHED();
                continue;
            }
            // Move the spanner back to its original position.
            auto& spannerOriginalParent = *placeholder->parent();
            // Detaching the spanner takes care of removing the placeholder (and merges the RenderMultiColumnSets).
            auto spannerToReInsert = m_builder.detach(*spanner->parent(), *spanner);
            m_builder.attach(spannerOriginalParent, WTFMove(spannerToReInsert));
        }
    }

    auto newFragmentedFlow = WebCore::createRenderer<RenderMultiColumnFlow>(flow.document(), RenderStyle::createAnonymousStyleWithDisplay(flow.style(), DisplayType::Block));
    newFragmentedFlow->initializeStyle();
    auto& fragmentedFlow = *newFragmentedFlow;
    m_builder.blockBuilder().attach(flow, WTFMove(newFragmentedFlow), nullptr);

    // Reparent children preceding the fragmented flow into the fragmented flow.
    m_builder.moveChildren(flow, fragmentedFlow, flow.firstChild(), &fragmentedFlow, RenderTreeBuilder::NormalizeAfterInsertion::Yes);
    if (flow.isFieldset()) {
        // Keep legends out of the flow thread.
        for (auto& box : childrenOfType<RenderBox>(fragmentedFlow)) {
            if (box.isLegend())
                m_builder.move(fragmentedFlow, flow, box, RenderTreeBuilder::NormalizeAfterInsertion::Yes);
        }
    }

    flow.setMultiColumnFlow(fragmentedFlow);
}

void RenderTreeBuilder::MultiColumn::destroyFragmentedFlow(RenderBlockFlow& flow)
{
    auto& multiColumnFlow = *flow.multiColumnFlow();
    multiColumnFlow.deleteLines();

    // Move spanners back to their original DOM position in the tree, and destroy the placeholders.
    auto& spanners = multiColumnFlow.spannerMap();
    Vector<RenderMultiColumnSpannerPlaceholder*> placeholdersToDelete;
    for (auto& spannerAndPlaceholder : spanners)
        placeholdersToDelete.append(spannerAndPlaceholder.value.get());
    Vector<std::pair<RenderElement*, RenderPtr<RenderObject>>> parentAndSpannerList;
    for (auto* placeholder : placeholdersToDelete) {
        auto* spannerOriginalParent = placeholder->parent();
        if (spannerOriginalParent == &multiColumnFlow)
            spannerOriginalParent = &flow;
        // Detaching the spanner takes care of removing the placeholder (and merges the RenderMultiColumnSets).
        auto* spanner = placeholder->spanner();
        parentAndSpannerList.append(std::make_pair(spannerOriginalParent, m_builder.detach(*spanner->parent(), *spanner)));
    }
    while (auto* columnSet = multiColumnFlow.firstMultiColumnSet())
        m_builder.destroy(*columnSet);

    flow.clearMultiColumnFlow();
    m_builder.moveAllChildren(multiColumnFlow, flow, RenderTreeBuilder::NormalizeAfterInsertion::Yes);
    m_builder.destroy(multiColumnFlow);
    for (auto& parentAndSpanner : parentAndSpannerList)
        m_builder.attach(*parentAndSpanner.first, WTFMove(parentAndSpanner.second));
}


RenderObject* RenderTreeBuilder::MultiColumn::resolveMovedChild(RenderFragmentedFlow& enclosingFragmentedFlow, RenderObject* beforeChild)
{
    if (!beforeChild)
        return nullptr;

    if (!is<RenderBox>(*beforeChild))
        return beforeChild;

    if (!is<RenderMultiColumnFlow>(enclosingFragmentedFlow))
        return beforeChild;

    // We only need to resolve for column spanners.
    if (beforeChild->style().columnSpan() != ColumnSpan::All)
        return beforeChild;

    // The renderer for the actual DOM node that establishes a spanner is moved from its original
    // location in the render tree to becoming a sibling of the column sets. In other words, it's
    // moved out from the flow thread (and becomes a sibling of it). When we for instance want to
    // create and insert a renderer for the sibling node immediately preceding the spanner, we need
    // to map that spanner renderer to the spanner's placeholder, which is where the new inserted
    // renderer belongs.
    if (auto* placeholder = downcast<RenderMultiColumnFlow>(enclosingFragmentedFlow).findColumnSpannerPlaceholder(downcast<RenderBox>(beforeChild)))
        return placeholder;

    // This is an invalid spanner, or its placeholder hasn't been created yet. This happens when
    // moving an entire subtree into the flow thread, when we are processing the insertion of this
    // spanner's preceding sibling, and we obviously haven't got as far as processing this spanner
    // yet.
    return beforeChild;
}

static bool gShiftingSpanner = false;

void RenderTreeBuilder::MultiColumn::multiColumnDescendantInserted(RenderMultiColumnFlow& flow, RenderObject& newDescendant)
{
    if (gShiftingSpanner || newDescendant.isInFlowRenderFragmentedFlow())
        return;

    auto* subtreeRoot = &newDescendant;
    auto* descendant = subtreeRoot;
    while (descendant) {
        // Skip nested multicolumn flows.
        if (is<RenderMultiColumnFlow>(*descendant)) {
            descendant = descendant->nextSibling();
            continue;
        }
        if (is<RenderMultiColumnSpannerPlaceholder>(*descendant)) {
            // A spanner's placeholder has been inserted. The actual spanner renderer is moved from
            // where it would otherwise occur (if it weren't a spanner) to becoming a sibling of the
            // column sets.
            RenderMultiColumnSpannerPlaceholder& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*descendant);
            ASSERT(!flow.spannerMap().get(placeholder.spanner()));
            flow.spannerMap().add(placeholder.spanner(), makeWeakPtr(downcast<RenderMultiColumnSpannerPlaceholder>(descendant)));
            ASSERT(!placeholder.firstChild()); // There should be no children here, but if there are, we ought to skip them.
        } else
            descendant = processPossibleSpannerDescendant(flow, subtreeRoot, *descendant);
        if (descendant)
            descendant = descendant->nextInPreOrder(subtreeRoot);
    }
}

RenderObject* RenderTreeBuilder::MultiColumn::processPossibleSpannerDescendant(RenderMultiColumnFlow& flow, RenderObject*& subtreeRoot, RenderObject& descendant)
{
    RenderBlockFlow* multicolContainer = flow.multiColumnBlockFlow();
    RenderObject* nextRendererInFragmentedFlow = spannerPlacehoderCandidate(descendant, flow);
    RenderObject* insertBeforeMulticolChild = nullptr;
    RenderObject* nextDescendant = &descendant;

    if (!multicolContainer)
        return nullptr;

    if (isValidColumnSpanner(flow, descendant)) {
        // This is a spanner (column-span:all). Such renderers are moved from where they would
        // otherwise occur in the render tree to becoming a direct child of the multicol container,
        // so that they live among the column sets. This simplifies the layout implementation, and
        // basically just relies on regular block layout done by the RenderBlockFlow that
        // establishes the multicol container.
        RenderBlockFlow* container = downcast<RenderBlockFlow>(descendant.parent());
        RenderMultiColumnSet* setToSplit = nullptr;
        if (nextRendererInFragmentedFlow) {
            setToSplit = findSetRendering(flow, descendant);
            if (setToSplit) {
                setToSplit->setNeedsLayout();
                insertBeforeMulticolChild = setToSplit->nextSibling();
            }
        }
        // Moving a spanner's renderer so that it becomes a sibling of the column sets requires us
        // to insert an anonymous placeholder in the tree where the spanner's renderer otherwise
        // would have been. This is needed for a two reasons: We need a way of separating inline
        // content before and after the spanner, so that it becomes separate line boxes. Secondly,
        // this placeholder serves as a break point for column sets, so that, when encountered, we
        // end flowing one column set and move to the next one.
        auto newPlaceholder = RenderMultiColumnSpannerPlaceholder::createAnonymous(flow, downcast<RenderBox>(descendant), container->style());
        auto& placeholder = *newPlaceholder;
        m_builder.attach(*container, WTFMove(newPlaceholder), descendant.nextSibling());
        auto takenDescendant = m_builder.detach(*container, descendant);

        // This is a guard to stop an ancestor flow thread from processing the spanner.
        gShiftingSpanner = true;
        m_builder.blockBuilder().attach(*multicolContainer, WTFMove(takenDescendant), insertBeforeMulticolChild);
        gShiftingSpanner = false;

        // The spanner has now been moved out from the flow thread, but we don't want to
        // examine its children anyway. They are all part of the spanner and shouldn't trigger
        // creation of column sets or anything like that. Continue at its original position in
        // the tree, i.e. where the placeholder was just put.
        if (subtreeRoot == &descendant)
            subtreeRoot = &placeholder;
        nextDescendant = &placeholder;
    } else {
        // This is regular multicol content, i.e. not part of a spanner.
        if (is<RenderMultiColumnSpannerPlaceholder>(nextRendererInFragmentedFlow)) {
            // Inserted right before a spanner. Is there a set for us there?
            RenderMultiColumnSpannerPlaceholder& placeholder = downcast<RenderMultiColumnSpannerPlaceholder>(*nextRendererInFragmentedFlow);
            if (RenderObject* previous = placeholder.spanner()->previousSibling()) {
                if (is<RenderMultiColumnSet>(*previous))
                    return nextDescendant; // There's already a set there. Nothing to do.
            }
            insertBeforeMulticolChild = placeholder.spanner();
        } else if (RenderMultiColumnSet* lastSet = flow.lastMultiColumnSet()) {
            // This child is not an immediate predecessor of a spanner, which means that if this
            // child precedes a spanner at all, there has to be a column set created for us there
            // already. If it doesn't precede any spanner at all, on the other hand, we need a
            // column set at the end of the multicol container. We don't really check here if the
            // child inserted precedes any spanner or not (as that's an expensive operation). Just
            // make sure we have a column set at the end. It's no big deal if it remains unused.
            if (!lastSet->nextSibling())
                return nextDescendant;
        }
    }
    // Need to create a new column set when there's no set already created. We also always insert
    // another column set after a spanner. Even if it turns out that there are no renderers
    // following the spanner, there may be bottom margins there, which take up space.
    auto newSet = createRenderer<RenderMultiColumnSet>(flow, RenderStyle::createAnonymousStyleWithDisplay(multicolContainer->style(), DisplayType::Block));
    newSet->initializeStyle();
    auto& set = *newSet;
    m_builder.blockBuilder().attach(*multicolContainer, WTFMove(newSet), insertBeforeMulticolChild);
    flow.invalidateFragments();

    // We cannot handle immediate column set siblings at the moment (and there's no need for
    // it, either). There has to be at least one spanner separating them.
    ASSERT_UNUSED(set, !RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(&set)
        || !RenderMultiColumnFlow::previousColumnSetOrSpannerSiblingOf(&set)->isRenderMultiColumnSet());
    ASSERT(!RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(&set)
        || !RenderMultiColumnFlow::nextColumnSetOrSpannerSiblingOf(&set)->isRenderMultiColumnSet());

    return nextDescendant;
}

void RenderTreeBuilder::MultiColumn::handleSpannerRemoval(RenderMultiColumnFlow& flow, RenderObject& spanner)
{
    // The placeholder may already have been removed, but if it hasn't, do so now.
    if (auto placeholder = flow.spannerMap().take(&downcast<RenderBox>(spanner)))
        m_builder.destroy(*placeholder);

    if (auto* next = spanner.nextSibling()) {
        if (auto* previous = spanner.previousSibling()) {
            if (previous->isRenderMultiColumnSet() && next->isRenderMultiColumnSet()) {
                // Merge two sets that no longer will be separated by a spanner.
                m_builder.destroy(*next);
                previous->setNeedsLayout();
            }
        }
    }
}

void RenderTreeBuilder::MultiColumn::multiColumnRelativeWillBeRemoved(RenderMultiColumnFlow& flow, RenderObject& relative)
{
    flow.invalidateFragments();
    if (is<RenderMultiColumnSpannerPlaceholder>(relative)) {
        // Remove the map entry for this spanner, but leave the actual spanner renderer alone. Also
        // keep the reference to the spanner, since the placeholder may be about to be re-inserted
        // in the tree.
        ASSERT(relative.isDescendantOf(&flow));
        flow.spannerMap().remove(downcast<RenderMultiColumnSpannerPlaceholder>(relative).spanner());
        return;
    }
    if (relative.style().columnSpan() == ColumnSpan::All) {
        if (relative.parent() != flow.parent())
            return; // not a valid spanner.

        handleSpannerRemoval(flow, relative);
    }
    // Note that we might end up with empty column sets if all column content is removed. That's no
    // big deal though (and locating them would be expensive), and they will be found and re-used if
    // content is added again later.
}

RenderObject* RenderTreeBuilder::MultiColumn::adjustBeforeChildForMultiColumnSpannerIfNeeded(RenderObject& beforeChild)
{
    if (!is<RenderBox>(beforeChild))
        return &beforeChild;

    auto* nextSibling = beforeChild.nextSibling();
    if (!nextSibling)
        return &beforeChild;

    if (!is<RenderMultiColumnSet>(*nextSibling))
        return &beforeChild;

    auto* multiColumnFlow = downcast<RenderMultiColumnSet>(*nextSibling).multiColumnFlow();
    if (!multiColumnFlow)
        return &beforeChild;

    return multiColumnFlow->findColumnSpannerPlaceholder(downcast<RenderBox>(&beforeChild));
}

}
