/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2025 Apple Inc. All rights reserved.
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
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderListMarker.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderObjectStyle.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderTreeUpdaterGeneratedContent.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle.h"
#include "StyleContent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::List);

struct LineBoxParentSearchResult {
    CheckedPtr<RenderBlock> parent;
    CheckedPtr<RenderBlock> fallbackParent;
    // FIXME: handle all block level children, not just replaced elements that got blockified.
    bool failedDueToBlockification { false };
};

static LineBoxParentSearchResult findParentOfEmptyOrFirstLineBox(RenderBlock& blockContainer, const RenderListMarker& marker)
{
    auto inQuirksMode = blockContainer.document().inQuirksMode();
    RenderBlock* fallbackParent = { };
    bool failedDueToBlockification = false;

    for (auto& child : childrenOfType<RenderObject>(blockContainer)) {
        if (&child == &marker)
            continue;

        if (child.isInline()) {
            // Continue searching past empty inlines (e.g., <a id="anchor"></a>) — the remaining checks below
            // assume block-level children, and an inline falling through would falsely trigger failedDueToBlockification.
            if (is<RenderInline>(child) && isEmptyInline(downcast<RenderInline>(child))) {
                fallbackParent = &blockContainer;
                continue;
            }
            if (is<RenderText>(child) && downcast<RenderText>(child).containsOnlyCollapsibleWhitespace())
                continue;
            return { &blockContainer, { }, false };
        }

        if (child.isFloating() || child.isOutOfFlowPositioned() || is<RenderMenuList>(child))
            continue;

        if (auto* renderBox = dynamicDowncast<RenderBox>(child); renderBox && renderBox->isWritingModeRoot())
            break;

        if (is<RenderListItem>(blockContainer) && inQuirksMode && child.node() && isHTMLListElement(*child.node()))
            break;

        if (!is<RenderBlock>(child) || is<RenderTable>(child) || child.style().display() == Style::DisplayType::BlockRuby) {
            failedDueToBlockification = true;
            break;
        }

        auto& blockChild = downcast<RenderBlock>(child);
        auto nestedResult = findParentOfEmptyOrFirstLineBox(blockChild, marker);
        if (nestedResult.parent) {
            // Finding a line box parent is mutually exclusive with blockification failure.
            ASSERT(!nestedResult.failedDueToBlockification);
            return { nestedResult.parent, { }, false };
        }

        // Propagate the blockification failure bit from nested searches so that we know whether the search failed due to blockification or because there was no inline content.
        failedDueToBlockification |= nestedResult.failedDueToBlockification;

        if (!fallbackParent) {
            if (nestedResult.fallbackParent)
                fallbackParent = nestedResult.fallbackParent;
            else if (auto* firstInFlowChild = blockChild.firstInFlowChild(); !firstInFlowChild || firstInFlowChild == &marker)
                fallbackParent = &blockChild;
        }
    }

    return { { }, fallbackParent, failedDueToBlockification };
}

struct MarkerParentSearchResult {
    CheckedPtr<RenderBlock> parent;
    bool shouldCollapseAnonymousBlockParent { false };
};

static MarkerParentSearchResult parentCandidateForMarker(RenderListItem& listItemRenderer, const RenderListMarker& marker)
{
    if (marker.isInside()) {
        if (auto* firstChild = dynamicDowncast<RenderBlock>(listItemRenderer.firstChild())) {
            if (!firstChild->isAnonymous())
                return { &listItemRenderer, false };
            // We may have created this anonymous block for the marker itself. Let's keep it in there.
            if (firstChild->firstChild() == &marker && !marker.nextSibling())
                return { firstChild, false };
        }
        auto result = findParentOfEmptyOrFirstLineBox(listItemRenderer, marker);
        return { result.parent, false };
    }
    auto result = findParentOfEmptyOrFirstLineBox(listItemRenderer, marker);
    return { result.parent ? result.parent : result.fallbackParent, result.failedDueToBlockification };
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

    if (listItemRenderer.element() && listItemRenderer.element()->hasTagName(HTMLNames::fieldsetTag)) {
        if (auto* marker = listItemRenderer.markerRenderer())
            m_builder.destroy(*marker);
        return;
    }

    auto newStyle = listItemRenderer.computeMarkerStyle();
    auto markerContentEnabled = listItemRenderer.document().settings().cssMarkerContentEnabled();
    auto markerHasContent = markerContentEnabled && newStyle.content().isData();

    // css-content-3: `content: none` on the ::marker suppresses the marker box entirely, regardless
    // of list-style-type/image. Otherwise (css-lists-3 §3.3) a non-normal `content` generates the
    // marker box even without a list-style-image/type; only suppress when there is nothing to show.
    // (Gated on the CSSMarkerContent feature; when disabled, `content` on ::marker is ignored.)
    RefPtr styleImage = style.listStyleImage().tryStyleImage();
    auto hasListStyle = !style.listStyleType().isNone() || (styleImage && !styleImage->errorOccurred());
    if ((markerContentEnabled && newStyle.content().isNone()) || (!markerHasContent && !hasListStyle)) {
        if (auto* marker = listItemRenderer.markerRenderer())
            m_builder.destroy(*marker);
        return;
    }

    if (auto* markerRenderer = listItemRenderer.markerRenderer()) {
        auto contentChanged = markerRenderer->style().content() != newStyle.content();
        // Tear down any existing generated content while the current style still permits children:
        // setStyle below may flip canHaveChildren() to false (content: normal/none), and destroying
        // the container afterwards would trip the detach assertion.
        if (contentChanged) {
            if (auto* existingContainer = markerRenderer->contentContainer())
                m_builder.destroy(*existingContainer);
        }
        markerRenderer->setStyle(WTF::move(newStyle));
        if (contentChanged) {
            if (markerHasContent)
                buildMarkerContentRenderers(*markerRenderer);
        } else if (auto* container = markerRenderer->contentContainer()) {
            // Content unchanged but other style changed: refresh the generated image/quote children
            // (RenderText/RenderCounter are handled by propagateStyleToAnonymousChildren on setStyle).
            RenderTreeUpdater::GeneratedContent::updateStyleForContentRenderers(*container, markerRenderer->style());
        }
        auto* currentParent = markerRenderer->parent();
        if (!currentParent) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto searchResult = parentCandidateForMarker(listItemRenderer, *markerRenderer);
        if (!searchResult.parent) {
            if (currentParent->isAnonymousBlock()) {
                // For outside markers, if the search failed because a flex/grid container blockified a replaced
                // child (e.g., <img>), we should collapse the anonymous block's height so it doesn't inflate the list item.
                markerRenderer->setShouldCollapseAnonymousBlockParent(searchResult.shouldCollapseAnonymousBlockParent);
                // If the marker is currently contained inside an anonymous box, we are the only item in that anonymous box
                // since no line box parent was found. It's ok to just leave the marker where it is in this case.
                return;
            }
            searchResult.parent = &listItemRenderer;
            if (auto* multiColumnFlow = listItemRenderer.multiColumnFlow())
                searchResult.parent = multiColumnFlow;
        }

        if (searchResult.parent == currentParent)
            return;

        m_builder.attach(*searchResult.parent, m_builder.detach(*currentParent, *markerRenderer, WillBeDestroyed::No, RenderTreeBuilder::CanCollapseAnonymousBlock::No), firstNonMarkerChild(*searchResult.parent));

        // If current parent is an anonymous block that has lost all its children, destroy it.
        if (currentParent->isAnonymousBlock() && !currentParent->firstChild()) {
            // Clear the CheckedPtr first because m_builder.destroy may delete the block that searchResult.parent points to.
            searchResult.parent = nullptr;
            m_builder.destroy(*currentParent);
        }
        return;
    }

    RenderPtr<RenderListMarker> newMarkerRenderer = WebCore::createRenderer<RenderListMarker>(listItemRenderer, WTF::move(newStyle));
    newMarkerRenderer->initializeStyle();
    listItemRenderer.setMarkerRenderer(*newMarkerRenderer);
    auto searchResult = parentCandidateForMarker(listItemRenderer, *newMarkerRenderer);
    auto shouldCollapseAnonymousBlockParent = !searchResult.parent && !newMarkerRenderer->isInside() && searchResult.shouldCollapseAnonymousBlockParent;
    if (!searchResult.parent) {
        searchResult.parent = &listItemRenderer;
        if (auto* multiColumnFlow = listItemRenderer.multiColumnFlow())
            searchResult.parent = multiColumnFlow;
    }
    m_builder.attach(*searchResult.parent, WTF::move(newMarkerRenderer), firstNonMarkerChild(*searchResult.parent));
    // For outside markers, if the search failed because a flex/grid container blockified a replaced
    // child (e.g., <img>), we should collapse the anonymous block's height so it doesn't inflate the list item.
    listItemRenderer.markerRenderer()->setShouldCollapseAnonymousBlockParent(shouldCollapseAnonymousBlockParent);

    if (markerHasContent)
        buildMarkerContentRenderers(*listItemRenderer.markerRenderer());
}

void RenderTreeBuilder::List::buildMarkerContentRenderers(RenderListMarker& marker)
{
    ASSERT(marker.style().content().isData());
    ASSERT(!marker.contentContainer());

    // css-lists-3 §3.3 generates the marker contents "exactly as for ::before": an anonymous
    // inline-block box holding the content list (strings, images, counters, quotes). The marker
    // (RenderListMarker) lays this box out and paints it as a single atomic inline.
    auto containerStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(marker.style(), Style::DisplayType::InlineFlowRoot);
    auto newContainer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, marker.document(), WTF::move(containerStyle));
    newContainer->initializeStyle();
    CheckedRef container = *newContainer;
    m_builder.attach(marker, WTF::move(newContainer));

    RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, container.get(), marker.style(), PseudoElementType::Marker);
}

}
