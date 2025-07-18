/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
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
 *
 */

#include "config.h"
#include "RenderTreeBuilderList.h"

#include "LegacyInlineIterator.h"
#include "LineInlineHeaders.h"
#include "RenderChildIterator.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderTable.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::List);

static std::pair<RenderBlock*, RenderBlock*> findParentOfEmptyOrFirstLineBox(RenderBlock& blockContainer, const RenderListMarker& marker)
{
    auto inQuirksMode = blockContainer.document().inQuirksMode();
    RenderBlock* fallbackParent = { };

    for (auto& child : childrenOfType<RenderObject>(blockContainer)) {
        if (&child == &marker)
            continue;

        if (child.isInline()) {
            if (!is<RenderInline>(child) || !isEmptyInline(downcast<RenderInline>(child)))
                return { &blockContainer, { } };
            fallbackParent = &blockContainer;
        }

        if (child.isFloating() || child.isOutOfFlowPositioned() || is<RenderMenuList>(child))
            continue;

        if (auto* renderBox = dynamicDowncast<RenderBox>(child); renderBox && renderBox->isWritingModeRoot())
            break;

        if (is<RenderListItem>(blockContainer) && inQuirksMode && child.node() && isHTMLListElement(*child.node()))
            break;

        if (!is<RenderBlock>(child) || is<RenderTable>(child))
            break;

        auto& blockChild = downcast<RenderBlock>(child);
        auto [ nestedParent, nestedFallbackParent ] = findParentOfEmptyOrFirstLineBox(blockChild, marker);
        if (nestedParent)
            return { nestedParent, { } };

        if (!fallbackParent) {
            if (nestedFallbackParent)
                fallbackParent = nestedFallbackParent;
            else if (auto* firstInFlowChild = blockChild.firstInFlowChild(); !firstInFlowChild || firstInFlowChild == &marker)
                fallbackParent = &blockChild;
        }
    }

    return { { }, fallbackParent };
}

static RenderBlock* parentCandidateForMarker(RenderListItem& listItemRenderer, const RenderListMarker& marker)
{
    if (marker.isInside()) {
        if (auto* firstChild = dynamicDowncast<RenderBlock>(listItemRenderer.firstChild()); firstChild && !firstChild->isAnonymous())
            return &listItemRenderer;
        return findParentOfEmptyOrFirstLineBox(listItemRenderer, marker).first;
    }
    auto [parentCandidate, fallbackParent] = findParentOfEmptyOrFirstLineBox(listItemRenderer, marker);
    return parentCandidate ? parentCandidate : fallbackParent;
}

static RenderObject* firstNonMarkerChild(RenderBlock& parent)
{
    RenderObject* child = parent.firstChild();
    while (is<RenderListMarker>(child))
        child = child->nextSibling();
    return child;
}

RenderTreeBuilder::List::List(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

void RenderTreeBuilder::List::updateItemMarker(RenderListItem& listItemRenderer)
{
    auto& style = listItemRenderer.style();

    if (style.listStyleType().isNone() && (!style.listStyleImage() || style.listStyleImage()->errorOccurred())) {
        if (auto* marker = listItemRenderer.markerRenderer())
            m_builder.destroy(*marker);
        return;
    }

    auto newStyle = listItemRenderer.computeMarkerStyle();
    if (auto* markerRenderer = listItemRenderer.markerRenderer()) {
        markerRenderer->setStyle(WTFMove(newStyle));
        auto* currentParent = markerRenderer->parent();
        if (!currentParent) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto* newParent = parentCandidateForMarker(listItemRenderer, *markerRenderer);
        if (!newParent) {
            if (currentParent->isAnonymousBlock()) {
                // If the marker is currently contained inside an anonymous box. then we are the only item in that anonymous box
                // (since no line box parent was found). It's ok to just leave the marker where it is in this case.
                return;
            }
            newParent = &listItemRenderer;
            if (auto* multiColumnFlow = listItemRenderer.multiColumnFlow())
                newParent = multiColumnFlow;
        }

        if (newParent == currentParent)
            return;

        m_builder.attach(*newParent, m_builder.detach(*currentParent, *markerRenderer, WillBeDestroyed::No, RenderTreeBuilder::CanCollapseAnonymousBlock::No), firstNonMarkerChild(*newParent));
        // If current parent is an anonymous block that has lost all its children, destroy it.
        if (currentParent->isAnonymousBlock() && !currentParent->firstChild() && !downcast<RenderBlock>(*currentParent).continuation())
            m_builder.destroy(*currentParent);
        return;
    }

    RenderPtr<RenderListMarker> newMarkerRenderer = WebCore::createRenderer<RenderListMarker>(listItemRenderer, WTFMove(newStyle));
    newMarkerRenderer->initializeStyle();
    listItemRenderer.setMarkerRenderer(*newMarkerRenderer);
    auto* newParent = parentCandidateForMarker(listItemRenderer, *newMarkerRenderer);
    if (!newParent) {
        // If the marker is currently contained inside an anonymous box,
        // then we are the only item in that anonymous box (since no line box
        // parent was found). It's ok to just leave the marker where it is
        // in this case.
        newParent = &listItemRenderer;
        if (auto* multiColumnFlow = listItemRenderer.multiColumnFlow())
            newParent = multiColumnFlow;
    }
    m_builder.attach(*newParent, WTFMove(newMarkerRenderer), firstNonMarkerChild(*newParent));
}

}
