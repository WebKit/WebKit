/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2006, 2010, 2017 Apple Inc. All rights reserved.
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
#include "RenderTreeUpdaterListItem.h"

#include "RenderChildIterator.h"
#include "RenderListMarker.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderRuby.h"
#include "RenderTable.h"

namespace WebCore {

static RenderBlock* getParentOfFirstLineBox(RenderBlock& current, RenderObject& marker)
{
    bool inQuirksMode = current.document().inQuirksMode();
    for (auto& child : childrenOfType<RenderObject>(current)) {
        if (&child == &marker)
            continue;

        if (child.isInline() && (!is<RenderInline>(child) || current.generatesLineBoxesForInlineChild(&child)))
            return &current;

        if (child.isFloating() || child.isOutOfFlowPositioned())
            continue;

        if (!is<RenderBlock>(child) || is<RenderTable>(child) || is<RenderRubyAsBlock>(child))
            break;

        if (is<RenderBox>(child) && downcast<RenderBox>(child).isWritingModeRoot())
            break;

        if (is<RenderListItem>(current) && inQuirksMode && child.node() && isHTMLListElement(*child.node()))
            break;

        if (RenderBlock* lineBox = getParentOfFirstLineBox(downcast<RenderBlock>(child), marker))
            return lineBox;
    }

    return nullptr;
}

static RenderObject* firstNonMarkerChild(RenderBlock& parent)
{
    RenderObject* child = parent.firstChild();
    while (is<RenderListMarker>(child))
        child = child->nextSibling();
    return child;
}

void RenderTreeUpdater::ListItem::updateMarker(RenderListItem& listItemRenderer)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!listItemRenderer.view().layoutState());

    auto& style = listItemRenderer.style();

    if (style.listStyleType() == NoneListStyle && (!style.listStyleImage() || style.listStyleImage()->errorOccurred())) {
        if (listItemRenderer.markerRenderer()) {
            listItemRenderer.markerRenderer()->destroy();
            ASSERT(!listItemRenderer.markerRenderer());
        }
        return;
    }

    auto newStyle = listItemRenderer.computeMarkerStyle();
    auto* markerRenderer = listItemRenderer.markerRenderer();
    if (markerRenderer)
        markerRenderer->setStyle(WTFMove(newStyle));
    else {
        markerRenderer = WebCore::createRenderer<RenderListMarker>(listItemRenderer, WTFMove(newStyle)).leakPtr();
        markerRenderer->initializeStyle();
        listItemRenderer.setMarkerRenderer(markerRenderer);
    }

    RenderElement* currentParent = markerRenderer->parent();
    RenderBlock* newParent = getParentOfFirstLineBox(listItemRenderer, *markerRenderer);
    if (!newParent) {
        // If the marker is currently contained inside an anonymous box,
        // then we are the only item in that anonymous box (since no line box
        // parent was found). It's ok to just leave the marker where it is
        // in this case.
        if (currentParent && currentParent->isAnonymousBlock())
            return;
        if (auto* multiColumnFlowThread = listItemRenderer.multiColumnFlowThread())
            newParent = multiColumnFlowThread;
        else
            newParent = &listItemRenderer;
    }

    if (newParent != currentParent) {
        markerRenderer->removeFromParent();
        newParent->addChild(markerRenderer, firstNonMarkerChild(*newParent));
        // If current parent is an anonymous block that has lost all its children, destroy it.
        if (currentParent && currentParent->isAnonymousBlock() && !currentParent->firstChild() && !downcast<RenderBlock>(*currentParent).continuation())
            currentParent->destroy();
    }
}

}
