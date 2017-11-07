/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
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
#include "RenderChildIterator.h"
#include "RenderInline.h"
#include "RenderListMarker.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/IsoMallocInlines.h>
#if !ASSERT_DISABLED
#include <wtf/SetForScope.h>
#endif
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderListItem);

RenderListItem::RenderListItem(Element& element, RenderStyle&& style)
    : RenderBlockFlow(element, WTFMove(style))
    , m_hasExplicitValue(false)
    , m_isValueUpToDate(false)
    , m_notInList(false)
{
    setInline(false);
}

RenderListItem::~RenderListItem()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
}

void RenderListItem::willBeDestroyed()
{
    if (m_marker)
        m_marker->removeFromParentAndDestroy();
    RenderBlockFlow::willBeDestroyed();
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
    parentStyle.setFontDescription(fontDescription);
    parentStyle.fontCascade().update(&document().fontSelector());

    if (auto markerStyle = getCachedPseudoStyle(MARKER, &parentStyle))
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
    Element& listItemElement = listItem.element();
    Element* parent = is<PseudoElement>(listItemElement) ? downcast<PseudoElement>(listItemElement).hostElement() : listItemElement.parentElement();
    Element* firstNode = parent;
    // We use parentNode because the enclosing list could be a ShadowRoot that's not Element.
    for (; parent; parent = parent->parentElement()) {
        if (isHTMLListElement(*parent))
            return parent;
    }

    // If there's no actual <ul> or <ol> list element, then the first found
    // node acts as our list for purposes of determining what other list items
    // should be numbered as part of the same list.
    return firstNode;
}

// Returns the next list item with respect to the DOM order.
static RenderListItem* nextListItem(const Element& listNode, const Element& element)
{
    for (const Element* next = ElementTraversal::nextIncludingPseudo(element, &listNode); next; ) {
        auto* renderer = next->renderer();
        if (!renderer || isHTMLListElement(*next)) {
            // We've found a nested, independent list or an unrendered Element : nothing to do here.
            next = ElementTraversal::nextIncludingPseudoSkippingChildren(*next, &listNode);
            continue;
        }

        if (is<RenderListItem>(*renderer))
            return downcast<RenderListItem>(renderer);

        next = ElementTraversal::nextIncludingPseudo(*next, &listNode);
    }

    return nullptr;
}

static inline RenderListItem* nextListItem(const Element& listNode, const RenderListItem& item)
{
    return nextListItem(listNode, item.element());
}

static inline RenderListItem* nextListItem(const Element& listNode)
{
    return nextListItem(listNode, listNode);
}

// Returns the previous list item with respect to the DOM order.
static RenderListItem* previousListItem(const Element* listNode, const RenderListItem& item)
{
    for (const Element* current = ElementTraversal::previousIncludingPseudo(item.element(), listNode); current; current = ElementTraversal::previousIncludingPseudo(*current, listNode)) {
        RenderElement* renderer = current->renderer();
        if (!is<RenderListItem>(renderer))
            continue;
        Element* otherList = enclosingList(downcast<RenderListItem>(*renderer));
        // This item is part of our current list, so it's what we're looking for.
        if (listNode == otherList)
            return downcast<RenderListItem>(renderer);
        // We found ourself inside another list; lets skip the rest of it.
        // Use nextIncludingPseudo() here because the other list itself may actually
        // be a list item itself. We need to examine it, so we do this to counteract
        // the previousIncludingPseudo() that will be done by the loop.
        if (otherList)
            current = ElementTraversal::nextIncludingPseudo(*otherList);
    }
    return nullptr;
}

void RenderListItem::updateItemValuesForOrderedList(const HTMLOListElement& listNode)
{
    for (RenderListItem* listItem = nextListItem(listNode); listItem; listItem = nextListItem(listNode, *listItem))
        listItem->updateValue();
}

unsigned RenderListItem::itemCountForOrderedList(const HTMLOListElement& listNode)
{
    unsigned itemCount = 0;
    for (RenderListItem* listItem = nextListItem(listNode); listItem; listItem = nextListItem(listNode, *listItem))
        ++itemCount;
    return itemCount;
}

inline int RenderListItem::calcValue() const
{
    if (m_hasExplicitValue)
        return m_explicitValue;

    Element* list = enclosingList(*this);
    HTMLOListElement* oListElement = is<HTMLOListElement>(list) ? downcast<HTMLOListElement>(list) : nullptr;
    int valueStep = 1;
    if (oListElement && oListElement->isReversed())
        valueStep = -1;

    // FIXME: This recurses to a possible depth of the length of the list.
    // That's not good -- we need to change this to an iterative algorithm.
    if (RenderListItem* previousItem = previousListItem(list, *this))
        return previousItem->value() + valueStep;

    if (oListElement)
        return oListElement->start();

    return 1;
}

void RenderListItem::updateValueNow() const
{
    m_value = calcValue();
    m_isValueUpToDate = true;
}

void RenderListItem::updateValue()
{
    if (!m_hasExplicitValue) {
        m_isValueUpToDate = false;
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
    positionListMarker();
    RenderBlockFlow::addOverflowFromChildren();
}

void RenderListItem::computePreferredLogicalWidths()
{
    // FIXME: RenderListMarker::updateMargins() mutates margin style which affects preferred widths.
    if (m_marker && m_marker->preferredLogicalWidthsDirty())
        m_marker->updateMarginsAndContent();

    RenderBlockFlow::computePreferredLogicalWidths();
}

void RenderListItem::positionListMarker()
{
    if (!m_marker || !m_marker->parent() || !m_marker->parent()->isBox())
        return;

    if (m_marker->isInside() || !m_marker->inlineBoxWrapper())
        return;

    LayoutUnit markerOldLogicalLeft = m_marker->logicalLeft();
    LayoutUnit blockOffset = 0;
    LayoutUnit lineOffset = 0;
    for (RenderBox* o = m_marker->parentBox(); o != this; o = o->parentBox()) {
        blockOffset += o->logicalTop();
        lineOffset += o->logicalLeft();
    }

    bool adjustOverflow = false;
    LayoutUnit markerLogicalLeft;
    bool hitSelfPaintingLayer = false;

    const RootInlineBox& rootBox = m_marker->inlineBoxWrapper()->root();
    LayoutUnit lineTop = rootBox.lineTop();
    LayoutUnit lineBottom = rootBox.lineBottom();

    // FIXME: Need to account for relative positioning in the layout overflow.
    if (style().isLeftToRightDirection()) {
        markerLogicalLeft = m_marker->lineOffsetForListItem() - lineOffset - paddingStart() - borderStart() + m_marker->marginStart();
        m_marker->inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (InlineFlowBox* box = m_marker->inlineBoxWrapper()->parent(); box; box = box->parent()) {
            LayoutRect newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            LayoutRect newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft < newLogicalVisualOverflowRect.x() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(newLogicalVisualOverflowRect.maxX() - markerLogicalLeft);
                newLogicalVisualOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft < newLogicalLayoutOverflowRect.x()) {
                newLogicalLayoutOverflowRect.setWidth(newLogicalLayoutOverflowRect.maxX() - markerLogicalLeft);
                newLogicalLayoutOverflowRect.setX(markerLogicalLeft);
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    } else {
        markerLogicalLeft = m_marker->lineOffsetForListItem() - lineOffset + paddingStart() + borderStart() + m_marker->marginEnd();
        m_marker->inlineBoxWrapper()->adjustLineDirectionPosition(markerLogicalLeft - markerOldLogicalLeft);
        for (InlineFlowBox* box = m_marker->inlineBoxWrapper()->parent(); box; box = box->parent()) {
            LayoutRect newLogicalVisualOverflowRect = box->logicalVisualOverflowRect(lineTop, lineBottom);
            LayoutRect newLogicalLayoutOverflowRect = box->logicalLayoutOverflowRect(lineTop, lineBottom);
            if (markerLogicalLeft + m_marker->logicalWidth() > newLogicalVisualOverflowRect.maxX() && !hitSelfPaintingLayer) {
                newLogicalVisualOverflowRect.setWidth(markerLogicalLeft + m_marker->logicalWidth() - newLogicalVisualOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            if (markerLogicalLeft + m_marker->logicalWidth() > newLogicalLayoutOverflowRect.maxX()) {
                newLogicalLayoutOverflowRect.setWidth(markerLogicalLeft + m_marker->logicalWidth() - newLogicalLayoutOverflowRect.x());
                if (box == &rootBox)
                    adjustOverflow = true;
            }
            box->setOverflowFromLogicalRects(newLogicalLayoutOverflowRect, newLogicalVisualOverflowRect, lineTop, lineBottom);
                
            if (box->renderer().hasSelfPaintingLayer())
                hitSelfPaintingLayer = true;
        }
    }

    if (adjustOverflow) {
        LayoutRect markerRect(markerLogicalLeft + lineOffset, blockOffset, m_marker->width(), m_marker->height());
        if (!style().isHorizontalWritingMode())
            markerRect = markerRect.transposedRect();
        RenderBox* markerAncestor = m_marker.get();
        bool propagateVisualOverflow = true;
        bool propagateLayoutOverflow = true;
        do {
            markerAncestor = markerAncestor->parentBox();
            if (markerAncestor->hasOverflowClip())
                propagateVisualOverflow = false;
            if (is<RenderBlock>(*markerAncestor)) {
                if (propagateVisualOverflow)
                    downcast<RenderBlock>(*markerAncestor).addVisualOverflow(markerRect);
                if (propagateLayoutOverflow)
                    downcast<RenderBlock>(*markerAncestor).addLayoutOverflow(markerRect);
            }
            if (markerAncestor->hasOverflowClip())
                propagateLayoutOverflow = false;
            if (markerAncestor->hasSelfPaintingLayer())
                propagateVisualOverflow = false;
            markerRect.moveBy(-markerAncestor->location());
        } while (markerAncestor != this && propagateVisualOverflow && propagateLayoutOverflow);
    }
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
    Element* listNode = enclosingList(*this);
    if (!listNode)
        return;
    for (RenderListItem* item = nextListItem(*listNode, *this); item; item = nextListItem(*listNode, *item))
        item->updateValue();
}

void RenderListItem::setExplicitValue(int value)
{
    if (m_hasExplicitValue && m_explicitValue == value)
        return;
    m_explicitValue = value;
    m_value = value;
    m_hasExplicitValue = true;
    explicitValueChanged();
}

void RenderListItem::clearExplicitValue()
{
    if (!m_hasExplicitValue)
        return;
    m_hasExplicitValue = false;
    m_isValueUpToDate = false;
    explicitValueChanged();
}

static inline RenderListItem* previousOrNextItem(bool isListReversed, Element& list, RenderListItem& item)
{
    return isListReversed ? previousListItem(&list, item) : nextListItem(list, item);
}

void RenderListItem::updateListMarkerNumbers()
{
    Element* listNode = enclosingList(*this);
    // The list node can be the shadow root which has no renderer.
    if (!listNode)
        return;

    bool isListReversed = false;
    if (is<HTMLOListElement>(*listNode)) {
        HTMLOListElement& oListElement = downcast<HTMLOListElement>(*listNode);
        oListElement.itemCountChanged();
        isListReversed = oListElement.isReversed();
    }
    for (RenderListItem* item = previousOrNextItem(isListReversed, *listNode, *this); item; item = previousOrNextItem(isListReversed, *listNode, *item)) {
        if (!item->m_isValueUpToDate) {
            // If an item has been marked for update before, we can safely
            // assume that all the following ones have too.
            // This gives us the opportunity to stop here and avoid
            // marking the same nodes again.
            break;
        }
        item->updateValue();
    }
}

} // namespace WebCore
