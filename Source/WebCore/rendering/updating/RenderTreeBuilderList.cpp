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

bool markerNeedsOwnLine(const RenderListItem& listItemRenderer)
{
    if (!listItemRenderer.document().inQuirksMode())
        return false;
    for (CheckedPtr child = listItemRenderer.firstChild(); child; child = child->nextSibling()) {
        if (child.get() == listItemRenderer.markerRenderer() || child->isFloatingOrOutOfFlowPositioned() || is<RenderMenuList>(*child))
            continue;
        return child->node() && isHTMLListElement(*child->node());
    }
    return false;
}

struct MarkerParentSearchResult {
    CheckedPtr<RenderBlock> parent;
    bool shouldCollapseAnonymousBlockParent { false };
};

static MarkerParentSearchResult parentCandidateForMarker(RenderListItem& listItemRenderer, const RenderListMarker& marker)
{
    if (!marker.isInside() && listItemRenderer.document().settings().listMarkerPositionedPostLayoutEnabled() && !markerNeedsOwnLine(listItemRenderer)) {
        // The outside marker is always the list item's first child and it takes no part in in-flow layout.
        return { &listItemRenderer, false };
    }

    if (marker.isInside()) {
        // Past the marker itself, for a list-style-position change from outside to inside: the marker the list item
        // was positioning is its own first child, so plain firstChild() would hand back the marker rather than the
        // content this is meant to look at, and the marker would be pushed down into a descendant instead of taking
        // a line at the start of the list item.
        if (auto* firstChild = dynamicDowncast<RenderBlock>(firstNonMarkerChild(listItemRenderer))) {
            if (!firstChild->isAnonymous())
                return { &listItemRenderer, false };
            // We may have created this anonymous block for the marker itself. Let's keep it in there.
            if (firstChild->firstChild() == &marker && !marker.nextSibling())
                return { firstChild, false };
        }
        auto result = RenderListItem::firstFormattedLineRootFor(listItemRenderer, marker);
        return { result.parent, false };
    }
    auto result = RenderListItem::firstFormattedLineRootFor(listItemRenderer, marker);
    return { result.parent ? result.parent : result.fallbackParent, result.stoppedAtTableRubyOrReplaced };
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

    // A list-style-position change moves the marker between two structurally different placements: an outside marker is
    // excluded and attaches directly to the list item, while an inside one is ordinary inline content that needs an
    // anonymous block when the list item's other children are block level. Only attach() makes that call, and it is not
    // consulted when the marker's parent happens to be unchanged, so rebuild rather than patch the placement in place.
    if (auto* markerRenderer = listItemRenderer.markerRenderer(); markerRenderer && markerRenderer->style().listStylePosition() != newStyle.listStylePosition())
        m_builder.destroyAndCleanUpAnonymousWrappers(*markerRenderer, { });

    if (auto* markerRenderer = listItemRenderer.markerRenderer()) {
        // Whether the marker box holds inline content depends on `content` and on list-style-type
        // (only text markers with right-to-left content need it). The container also inherits
        // unicode-bidi, so rebuild on that too, to carry the new value into it.
        auto contentChanged = markerRenderer->style().content() != newStyle.content() || markerRenderer->style().unicodeBidi() != newStyle.unicodeBidi() || markerRenderer->style().listStyleType() != newStyle.listStyleType();
        markerRenderer->setStyle(WTF::move(newStyle));

        // Recomputing this here rather than diffing the style properties it is made of also picks up
        // what is not the marker's own style: its direction, and what the document's counter style
        // registry currently resolves list-style-type to.
        auto needsContentContainer = markerRenderer->needsContentContainer();
        if (contentChanged || needsContentContainer != !!markerRenderer->contentContainer()) {
            if (auto* existingContainer = markerRenderer->contentContainer())
                m_builder.destroy(*existingContainer);
            if (needsContentContainer)
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
        markerRenderer->setIsExcludedFromNormalLayout(false);
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

    if (listItemRenderer.markerRenderer()->needsContentContainer())
        buildMarkerContentRenderers(*listItemRenderer.markerRenderer());
}

void RenderTreeBuilder::List::buildMarkerContentRenderers(RenderListMarker& marker)
{
    ASSERT(marker.needsContentContainer());
    ASSERT(!marker.contentContainer());

    // css-lists-3 §3.3 generates the marker contents "exactly as for ::before": an anonymous
    // inline-block box holding the content list (strings, images, counters, quotes). The marker
    // (RenderListMarker) lays this box out and paints it as a single atomic inline.
    auto containerStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(marker.style(), Style::DisplayType::InlineFlowRoot);
    auto newContainer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, marker.document(), WTF::move(containerStyle));
    newContainer->initializeStyle();
    CheckedRef container = *newContainer;
    m_builder.attach(marker, WTF::move(newContainer));

    if (!marker.hasContentProperty()) {
        // list-style-type text that needs renderers of its own, so inline layout can bidi-resolve it.
        // The text itself is only known once counter values resolve, so start empty and let
        // RenderListMarker::updateContent() fill it in at layout time.
        auto textRenderer = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, marker.document(), emptyString());
        m_builder.attach(container.get(), WTF::move(textRenderer));
        return;
    }

    RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, container.get(), marker.style(), PseudoElementType::Marker);
}

}
