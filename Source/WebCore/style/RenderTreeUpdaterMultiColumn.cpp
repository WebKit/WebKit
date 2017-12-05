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
#include "RenderTreeUpdaterMultiColumn.h"

#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSet.h"
#include "RenderMultiColumnSpannerPlaceholder.h"

namespace WebCore {

void RenderTreeUpdater::MultiColumn::update(RenderBlockFlow& flow)
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

void RenderTreeUpdater::MultiColumn::createFragmentedFlow(RenderBlockFlow& flow)
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
            auto spannerToReInsert = spanner->parent()->takeChild(*spanner);
            spannerOriginalParent.addChild(WTFMove(spannerToReInsert));
        }
    }

    auto newFragmentedFlow = WebCore::createRenderer<RenderMultiColumnFlow>(flow.document(), RenderStyle::createAnonymousStyleWithDisplay(flow.style(), BLOCK));
    newFragmentedFlow->initializeStyle();
    auto& fragmentedFlow = *newFragmentedFlow;
    flow.RenderBlock::addChild(WTFMove(newFragmentedFlow));

    // Reparent children preceding the fragmented flow into the fragmented flow.
    flow.moveChildrenTo(&fragmentedFlow, flow.firstChild(), &fragmentedFlow, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
    if (flow.isFieldset()) {
        // Keep legends out of the flow thread.
        for (auto& box : childrenOfType<RenderBox>(fragmentedFlow)) {
            if (box.isLegend())
                fragmentedFlow.moveChildTo(&flow, &box, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
        }
    }

    flow.setMultiColumnFlow(fragmentedFlow);
}

void RenderTreeUpdater::MultiColumn::destroyFragmentedFlow(RenderBlockFlow& flow)
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
        parentAndSpannerList.append(std::make_pair(spannerOriginalParent, spanner->parent()->takeChild(*spanner)));
    }
    while (auto* columnSet = multiColumnFlow.firstMultiColumnSet())
        columnSet->removeFromParentAndDestroy();

    flow.clearMultiColumnFlow();
    multiColumnFlow.moveAllChildrenTo(&flow, RenderBoxModelObject::NormalizeAfterInsertion::Yes);
    multiColumnFlow.removeFromParentAndDestroy();
    for (auto& parentAndSpanner : parentAndSpannerList)
        parentAndSpanner.first->addChild(WTFMove(parentAndSpanner.second));
}

}
