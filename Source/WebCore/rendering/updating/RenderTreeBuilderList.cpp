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
#include "RenderImage.h"
#include "RenderListOutsideMarker.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderObjectStyle.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderTreeUpdaterGeneratedContent.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleComputedStyle.h"
#include "StyleContent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderTreeBuilder::List);

static RenderObject* firstNonMarkerChild(RenderBlock& parent)
{
    RenderObject* child = parent.firstChild();
    while (is<RenderListOutsideMarker>(child))
        child = child->nextSibling();
    return child;
}

RenderTreeBuilder::List::List(RenderTreeBuilder& builder)
    : m_builder(builder)
{
}

struct MarkerParentSearchResult {
    CheckedPtr<RenderBlock> parent;
};

static MarkerParentSearchResult parentCandidateForMarker(RenderListItem& listItemRenderer, const RenderListOutsideMarker& marker)
{
    if (listItemRenderer.document().settings().listMarkerPositionedPostLayoutEnabled()) {
        // The outside marker is always the list item's first child and it takes no part in in-flow layout.
        return { &listItemRenderer };
    }

    auto result = RenderListItem::firstFormattedLineRootFor(listItemRenderer, marker);
    return { result.parent ? result.parent : result.fallbackParent };
}

// An inside marker among the list item's own inline content is not a box of its own: it is an anonymous inline box
// holding the marker's content renderers, so that they are inline content of the list item's formatting context and are
// laid out, reordered and painted with the rest of the line.
static void adjustStyleForInlineMarker(Style::ComputedStyle& markerStyle)
{
    markerStyle.setDisplay(Style::DisplayType::InlineFlow);
}

void RenderTreeBuilder::List::updateItemMarker(RenderListItem& listItemRenderer)
{
    auto& style = listItemRenderer.style();

    auto destroyExistingMarker = [&] {
        if (auto* marker = listItemRenderer.markerBox())
            m_builder.destroy(*marker);
        else if (auto* inlineMarker = listItemRenderer.markerRenderer())
            m_builder.destroyAndCleanUpAnonymousWrappers(*inlineMarker, { });
    };

    if (listItemRenderer.element() && listItemRenderer.element()->hasTagName(HTMLNames::fieldsetTag)) {
        destroyExistingMarker();
        return;
    }

    auto newStyle = listItemRenderer.computeMarkerStyle();
    auto markerContentEnabled = listItemRenderer.document().settings().cssMarkerContentEnabled();
    auto markerHasContent = listMarkerHasContent(newStyle, listItemRenderer.document());

    // css-content-3: `content: none` on the ::marker suppresses the marker box entirely, regardless
    // of list-style-type/image. Otherwise (css-lists-3 §3.3) a non-normal `content` generates the
    // marker box even without a list-style-image/type; only suppress when there is nothing to show.
    // (Gated on the CSSMarkerContent feature; when disabled, `content` on ::marker is ignored.)
    RefPtr styleImage = style.listStyleImage().tryStyleImage();
    auto hasListStyle = !style.listStyleType().isNone() || (styleImage && !styleImage->errorOccurred());
    if ((markerContentEnabled && newStyle.content().isNone()) || (!markerHasContent && !hasListStyle)) {
        destroyExistingMarker();
        return;
    }

    // A list-style-position change moves the marker between two structurally different placements: an outside marker is
    // excluded and attaches directly to the list item, while an inside one is ordinary inline content that needs an
    // anonymous block when the list item's other children are block level. Only attach() makes that call, and it is not
    // consulted when the marker's parent happens to be unchanged, so rebuild rather than patch the placement in place.
    auto shouldBuildInlineMarker = newStyle.listStylePosition() == ListStylePosition::Inside;

    if (CheckedPtr inlineMarker = listItemRenderer.markerRenderer(); inlineMarker && !listItemRenderer.markerBox()) {
        auto contentChanged = inlineMarker->style().content() != newStyle.content();
        if (shouldBuildInlineMarker && !contentChanged) {
            adjustStyleForInlineMarker(newStyle);
            inlineMarker->setStyle(WTF::move(newStyle));
            m_builder.addListItemNeedingMarkerUpdate(listItemRenderer);
            return;
        }
        m_builder.destroyAndCleanUpAnonymousWrappers(*inlineMarker, { });
    }

    if (auto* markerRenderer = listItemRenderer.markerBox(); markerRenderer && markerRenderer->style().listStylePosition() != newStyle.listStylePosition())
        m_builder.destroyAndCleanUpAnonymousWrappers(*markerRenderer, { });

    if (auto* markerRenderer = listItemRenderer.markerBox()) {
        auto contentChanged = markerRenderer->style().content() != newStyle.content() || markerRenderer->style().unicodeBidi() != newStyle.unicodeBidi()
            || markerRenderer->style().listStyleType() != newStyle.listStyleType() || markerRenderer->style().listStyleImage() != newStyle.listStyleImage();
        markerRenderer->setStyle(WTF::move(newStyle));
        // list-style-type, the counter style it resolves to and the writing mode all decide the marker's text.
        m_builder.addListItemNeedingMarkerUpdate(listItemRenderer);

        if (contentChanged) {
            if (auto* existingContainer = markerRenderer->contentContainer())
                m_builder.destroy(*existingContainer);
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

    if (shouldBuildInlineMarker) {
        buildInlineMarker(listItemRenderer, WTF::move(newStyle));
        return;
    }

    RenderPtr<RenderListOutsideMarker> newMarkerRenderer = WebCore::createRenderer<RenderListOutsideMarker>(listItemRenderer, WTF::move(newStyle));
    newMarkerRenderer->initializeStyle();
    m_builder.addListItemNeedingMarkerUpdate(listItemRenderer);
    listItemRenderer.setMarkerRenderer(*newMarkerRenderer);
    auto searchResult = parentCandidateForMarker(listItemRenderer, *newMarkerRenderer);
    if (!searchResult.parent) {
        searchResult.parent = &listItemRenderer;
        if (auto* multiColumnFlow = listItemRenderer.multiColumnFlow())
            searchResult.parent = multiColumnFlow;
    }
    m_builder.attach(*searchResult.parent, WTF::move(newMarkerRenderer), firstNonMarkerChild(*searchResult.parent));

    buildMarkerContentRenderers(*listItemRenderer.markerBox());
}

static CheckedRef<RenderBlock> parentForInlineMarker(RenderListItem& listItemRenderer, const RenderInline& marker)
{
    if (CheckedPtr firstChild = dynamicDowncast<RenderBlock>(firstNonMarkerChild(listItemRenderer)); firstChild && !firstChild->isAnonymous())
        return listItemRenderer;
    auto firstFormattedLineRoot = RenderListItem::firstFormattedLineRootFor(listItemRenderer, marker);
    if (firstFormattedLineRoot.parent)
        return *firstFormattedLineRoot.parent;
    return listItemRenderer;
}

void RenderTreeBuilder::List::buildInlineMarker(RenderListItem& listItemRenderer, Style::ComputedStyle&& markerStyle)
{
    adjustStyleForInlineMarker(markerStyle);
    auto newMarker = WebCore::createRenderer<RenderInline>(RenderObject::Type::Inline, listItemRenderer.document(), WTF::move(markerStyle));
    newMarker->initializeStyle();
    CheckedRef marker = *newMarker;
    CheckedRef markerParent = parentForInlineMarker(listItemRenderer, marker.get());
    m_builder.attach(markerParent.get(), WTF::move(newMarker), firstNonMarkerChild(markerParent.get()));
    listItemRenderer.setMarkerRenderer(marker.get());
    // What the marker shows is only known once counter values resolve, so leave it to the builder to fill in when the
    // tree is done changing, the same way the marker box's content is filled in.
    m_builder.addListItemNeedingMarkerUpdate(listItemRenderer);

    if (marker->style().content().isData()) {
        RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, marker.get(), marker->style(), PseudoElementType::Marker);
        return;
    }

    // css-lists-3 §3.3: a list-style-image draws in place of the counter style's text, as an image of its own.
    if (RefPtr styleImage = marker->style().listStyleImage().tryStyleImage(); styleImage && !styleImage->errorOccurred()) {
        auto imageRenderer = WebCore::createRenderer<RenderImage>(RenderObject::Type::Image, listItemRenderer.document(), Style::ComputedStyle::createStyleInheritingFromPseudoStyle(marker->style()), styleImage.get());
        imageRenderer->initializeStyle();
        m_builder.attach(marker.get(), WTF::move(imageRenderer));
        return;
    }

    auto textRenderer = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, listItemRenderer.document(), emptyString());
    m_builder.attach(marker.get(), WTF::move(textRenderer));
}

void RenderTreeBuilder::List::buildMarkerContentRenderers(RenderListOutsideMarker& marker)
{
    ASSERT(!marker.contentContainer());

    // css-lists-3 §3.3 generates the marker contents "exactly as for ::before": an anonymous
    // inline-block box holding the content list (strings, images, counters, quotes). The marker
    // (RenderListOutsideMarker) lays this box out and paints it as a single atomic inline.
    auto containerStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(marker.style(), Style::DisplayType::InlineFlowRoot);
    auto newContainer = WebCore::createRenderer<RenderBlockFlow>(RenderObject::Type::BlockFlow, marker.document(), WTF::move(containerStyle));
    newContainer->initializeStyle();
    CheckedRef container = *newContainer;
    m_builder.attach(marker, WTF::move(newContainer));

    if (!marker.hasContentProperty()) {
        // css-lists-3 §3.3: a list-style-image draws in place of the counter style's text, as an image of its own.
        if (RefPtr styleImage = marker.style().listStyleImage().tryStyleImage(); styleImage && !styleImage->errorOccurred()) {
            auto imageRenderer = WebCore::createRenderer<RenderImage>(RenderObject::Type::Image, marker.document(), Style::ComputedStyle::createStyleInheritingFromPseudoStyle(marker.style()), styleImage.get());
            imageRenderer->initializeStyle();
            m_builder.attach(container.get(), WTF::move(imageRenderer));
            return;
        }

        // list-style-type text that needs renderers of its own, so inline layout can bidi-resolve it.
        // The text itself is only known once counter values resolve, so start empty and let
        // RenderListOutsideMarker::updateContent() fill it in at layout time.
        auto textRenderer = WebCore::createRenderer<RenderText>(RenderObject::Type::Text, marker.document(), emptyString());
        m_builder.attach(container.get(), WTF::move(textRenderer));
        return;
    }

    RenderTreeUpdater::GeneratedContent::createContentRenderers(m_builder, container.get(), marker.style(), PseudoElementType::Marker);
}

}
