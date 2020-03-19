/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#include "RenderListItem.h"

#include "CSSFontSelector.h"
#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include "InlineElementBox.h"
#include "PseudoElement.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListItem);

RenderListItem::RenderListItem(Element& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
{
    setInline(false);
}

RenderListItem::~RenderListItem()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!m_marker);
}

RenderStyle RenderListItem::computeMarkerStyle() const
{
    // The marker always inherits from the list item, regardless of where it might end
    // up (e.g., in some deeply nested line box). See CSS3 spec.
    // FIXME: The marker should only inherit all font properties and the color property
    // according to the CSS Pseudo-Elements Module Level 4 spec.
    //
    // Although the CSS Pseudo-Elements Module Level 4 spec. saids to add ::marker to the UA sheet
    // we apply it here as an optimization because it only applies to markers. That is, it does not
    // apply to all elements.
    RenderStyle parentStyle = RenderStyle::clone(style());
    auto fontDescription = style().fontDescription();
    fontDescription.setVariantNumericSpacing(FontVariantNumericSpacing::TabularNumbers);
    parentStyle.setFontDescription(WTFMove(fontDescription));
    parentStyle.fontCascade().update(&document().fontSelector());
    if (auto markerStyle = getCachedPseudoStyle(PseudoId::Marker, &parentStyle))
        return RenderStyle::clone(*markerStyle);
    auto markerStyle = RenderStyle::create();
    markerStyle.inheritFrom(parentStyle);
    return markerStyle;
}

void RenderListItem::insertedIntoTree()
{
    RenderBlockFlow::insertedIntoTree();

    updateListMarkerNumbers();
}

void RenderListItem::willBeRemovedFromTree()
{
    RenderBlockFlow::willBeRemovedFromTree();

    updateListMarkerNumbers();
}

bool isHTMLListElement(const Node& node)
{
    return is<HTMLUListElement>(node) || is<HTMLOListElement>(node);
}

// Returns the enclosing list with respect to the DOM order.
static Element* enclosingList(const RenderListItem& listItem)
{
    auto& element = listItem.element();
    auto* parent = is<PseudoElement>(element) ? downcast<PseudoElement>(element).hostElement() : element.parentElement();
    for (auto* ancestor = parent; ancestor; ancestor = ancestor->parentElement()) {
        if (isHTMLListElement(*ancestor))
            return ancestor;
    }

    // If there's no actual list element, then the parent element acts as our
    // list for purposes of determining what other list items should be numbered as
    // part of the same list.
    return parent;
}

static RenderListItem* nextListItemHelper(const Element& list, const Element& element)
{
    auto* current = &element;
    auto advance = [&] {
        current = ElementTraversal::nextIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* renderer = current->renderer();
        if (!is<RenderListItem>(renderer)) {
            advance();
            continue;
        }
        auto& item = downcast<RenderListItem>(*renderer);
        auto* otherList = enclosingList(item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so it's what we're looking for.
        if (&list == otherList)
            return &item;

        // We found ourself inside another list; skip the rest of its contents.
        current = ElementTraversal::nextIncludingPseudoSkippingChildren(*current, &list);
    }

    return nullptr;
}

static inline RenderListItem* nextListItem(const Element& list, const RenderListItem& item)
{
    return nextListItemHelper(list, item.element());
}

static inline RenderListItem* firstListItem(const Element& list)
{
    return nextListItemHelper(list, list);
}

static RenderListItem* previousListItem(const Element& list, const RenderListItem& item)
{
    auto* current = &item.element();
    auto advance = [&] {
        current = ElementTraversal::previousIncludingPseudo(*current, &list);
    };
    advance();
    while (current) {
        auto* renderer = current->renderer();
        if (!is<RenderListItem>(renderer)) {
            advance();
            continue;
        }
        auto& item = downcast<RenderListItem>(*renderer);
        auto* otherList = enclosingList(item);
        if (!otherList) {
            advance();
            continue;
        }

        // This item is part of our current list, so we found what we're looking for.
        if (&list == otherList)
            return &item;

        // We found ourself inside another list; skip the rest of its contents by
        // advancing to it. However, since the list itself might be a list item,
        // don't advance past it.
        current = otherList;
    }
    return nullptr;
}

void RenderListItem::updateItemValuesForOrderedList(const HTMLOListElement& list)
{
    for (auto* listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        listItem->updateValue();
}

unsigned RenderListItem::itemCountForOrderedList(const HTMLOListElement& list)
{
    unsigned itemCount = 0;
    for (auto* listItem = firstListItem(list); listItem; listItem = nextListItem(list, *listItem))
        ++itemCount;
    return itemCount;
}

void RenderListItem::updateValueNow() const
{
    auto* list = enclosingList(*this);
    auto* orderedList = is<HTMLOListElement>(list) ? downcast<HTMLOListElement>(list) : nullptr;

    // The start item is either the closest item before this one in the list that already has a value,
    // or the first item in the list if none have before this have values yet.
    auto* startItem = this;
    if (list) {
        auto* item = this;
        while ((item = previousListItem(*list, *item))) {
            startItem = item;
            if (item->m_value)
                break;
        }
    }

    auto& startValue = startItem->m_value;
    if (!startValue)
        startValue = orderedList ? orderedList->start() : 1;
    int value = *startValue;
    int increment = (orderedList && orderedList->isReversed()) ? -1 : 1;

    for (auto* item = startItem; item != this; ) {
        item = nextListItem(*list, *item);
        item->m_value = (value += increment);
    }
}

void RenderListItem::updateValue()
{
    if (!m_valueWasSetExplicitly) {
        m_value = WTF::nullopt;
        if (m_marker)
            m_marker->setNeedsLayoutAndPrefWidthsRecalc();
    }
}

void RenderListItem::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    RenderBlockFlow::layout();
}

void RenderListItem::addOverflowFromChildren()
{
    if (m_marker)
        m_marker->addOverflowFromListMarker();
    RenderBlockFlow::addOverflowFromChildren();
}

void RenderListItem::computePreferredLogicalWidths()
{
    // FIXME: RenderListMarker::updateMargins() mutates margin style which affects preferred widths.
    if (m_marker && m_marker->preferredLogicalWidthsDirty())
        m_marker->updateMarginsAndContent();

    RenderBlockFlow::computePreferredLogicalWidths();
}

void RenderListItem::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (!logicalHeight() && hasOverflowClip())
        return;

    RenderBlockFlow::paint(paintInfo, paintOffset);
}

const String& RenderListItem::markerText() const
{
    if (m_marker)
        return m_marker->text();
    return nullAtom().string();
}

String RenderListItem::markerTextWithSuffix() const
{
    if (!m_marker)
        return String();

    // Append the suffix for the marker in the right place depending
    // on the direction of the text (right-to-left or left-to-right).
    if (m_marker->style().isLeftToRightDirection())
        return m_marker->text() + m_marker->suffix();
    return m_marker->suffix() + m_marker->text();
}

void RenderListItem::explicitValueChanged()
{
    if (m_marker)
        m_marker->setNeedsLayoutAndPrefWidthsRecalc();

    updateValue();
    auto* list = enclosingList(*this);
    if (!list)
        return;
    auto* item = this;
    while ((item = nextListItem(*list, *item)))
        item->updateValue();
}

void RenderListItem::setExplicitValue(Optional<int> value)
{
    if (!value) {
        if (!m_valueWasSetExplicitly)
            return;
    } else {
        if (m_valueWasSetExplicitly && m_value == value)
            return;
    }
    m_valueWasSetExplicitly = value.hasValue();
    m_value = value;
    explicitValueChanged();
}

void RenderListItem::updateListMarkerNumbers()
{
    auto* list = enclosingList(*this);
    if (!list)
        return;

    bool isInReversedOrderedList = false;
    if (is<HTMLOListElement>(*list)) {
        auto& orderedList = downcast<HTMLOListElement>(*list);
        orderedList.itemCountChanged();
        isInReversedOrderedList = orderedList.isReversed();
    }

    // If an item has been marked for update before, we know that all following items have, too.
    // This gives us the opportunity to stop and avoid marking the same nodes again.
    auto* item = this;
    auto subsequentListItem = isInReversedOrderedList ? previousListItem : nextListItem;
    while ((item = subsequentListItem(*list, *item)) && item->m_value)
        item->updateValue();
}

bool RenderListItem::isInReversedOrderedList() const
{
    auto* list = enclosingList(*this);
    return is<HTMLOListElement>(list) && downcast<HTMLOListElement>(*list).isReversed();
}

} // namespace WebCore
