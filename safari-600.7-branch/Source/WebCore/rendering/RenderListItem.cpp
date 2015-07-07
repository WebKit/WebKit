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

#include "ElementTraversal.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "InlineElementBox.h"
#include "PseudoElement.h"
#include "RenderListMarker.h"
#include "RenderMultiColumnFlowThread.h"
#include "RenderView.h"
#include "StyleInheritedData.h"
#include <wtf/StackStats.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

using namespace HTMLNames;

RenderListItem::RenderListItem(Element& element, PassRef<RenderStyle> style)
    : RenderBlockFlow(element, WTF::move(style))
    , m_hasExplicitValue(false)
    , m_isValueUpToDate(false)
    , m_notInList(false)
{
    setInline(false);
}

void RenderListItem::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlockFlow::styleDidChange(diff, oldStyle);

    if (style().listStyleType() == NoneListStyle && (!style().listStyleImage() || style().listStyleImage()->errorOccurred())) {
        m_marker = nullptr;
        return;
    }

    auto newStyle = RenderStyle::create();
    // The marker always inherits from the list item, regardless of where it might end
    // up (e.g., in some deeply nested line box). See CSS3 spec.
    newStyle.get().inheritFrom(&style());
    if (!m_marker) {
        m_marker = createRenderer<RenderListMarker>(*this, WTF::move(newStyle));
        m_marker->initializeStyle();
    } else
        m_marker->setStyle(WTF::move(newStyle));
}

void RenderListItem::willBeDestroyed()
{
    m_marker = nullptr;
    RenderBlockFlow::willBeDestroyed();
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

static bool isList(const Element* element)
{
    return element->hasTagName(ulTag) || element->hasTagName(olTag);
}

// Returns the enclosing list with respect to the DOM order.
static Element* enclosingList(const RenderListItem* listItem)
{
    Element& listItemElement = listItem->element();
    Element* firstNode = 0;
    Element* parent = listItemElement.isPseudoElement() ? toPseudoElement(listItemElement).hostElement() : listItemElement.parentElement();
    // We use parentNode because the enclosing list could be a ShadowRoot that's not Element.
    for (; parent; parent = parent->parentElement()) {
        if (isList(parent))
            return parent;
        if (!firstNode)
            firstNode = parent;
    }

    // If there's no actual <ul> or <ol> list element, then the first found
    // node acts as our list for purposes of determining what other list items
    // should be numbered as part of the same list.
    return firstNode;
}

// Returns the next list item with respect to the DOM order.
static RenderListItem* nextListItem(const Element* listNode, const RenderListItem* item = 0)
{
    if (!listNode)
        return 0;

    const Element* current = item ? &item->element() : listNode;
    current = ElementTraversal::nextIncludingPseudo(current, listNode);

    while (current) {
        if (isList(current)) {
            // We've found a nested, independent list: nothing to do here.
            current = ElementTraversal::nextIncludingPseudoSkippingChildren(current, listNode);
            continue;
        }

        RenderObject* renderer = current->renderer();
        if (renderer && renderer->isListItem())
            return toRenderListItem(renderer);

        // FIXME: Can this be optimized to skip the children of the elements without a renderer?
        current = ElementTraversal::nextIncludingPseudo(current, listNode);
    }

    return 0;
}

// Returns the previous list item with respect to the DOM order.
static RenderListItem* previousListItem(const Element* listNode, const RenderListItem* item)
{
    Element* current = &item->element();
    for (current = ElementTraversal::previousIncludingPseudo(current, listNode); current; current = ElementTraversal::previousIncludingPseudo(current, listNode)) {
        RenderObject* renderer = current->renderer();
        if (!renderer || (renderer && !renderer->isListItem()))
            continue;
        Node* otherList = enclosingList(toRenderListItem(renderer));
        // This item is part of our current list, so it's what we're looking for.
        if (listNode == otherList)
            return toRenderListItem(renderer);
        // We found ourself inside another list; lets skip the rest of it.
        // Use nextIncludingPseudo() here because the other list itself may actually
        // be a list item itself. We need to examine it, so we do this to counteract
        // the previousIncludingPseudo() that will be done by the loop.
        if (otherList)
            current = ElementTraversal::nextIncludingPseudo(otherList);
    }
    return 0;
}

void RenderListItem::updateItemValuesForOrderedList(const HTMLOListElement* listNode)
{
    ASSERT(listNode);

    for (RenderListItem* listItem = nextListItem(listNode); listItem; listItem = nextListItem(listNode, listItem))
        listItem->updateValue();
}

unsigned RenderListItem::itemCountForOrderedList(const HTMLOListElement* listNode)
{
    ASSERT(listNode);

    unsigned itemCount = 0;
    for (RenderListItem* listItem = nextListItem(listNode); listItem; listItem = nextListItem(listNode, listItem))
        itemCount++;

    return itemCount;
}

inline int RenderListItem::calcValue() const
{
    if (m_hasExplicitValue)
        return m_explicitValue;

    Element* list = enclosingList(this);
    HTMLOListElement* oListElement = (list && list->hasTagName(olTag)) ? toHTMLOListElement(list) : nullptr;
    int valueStep = 1;
    if (oListElement && oListElement->isReversed())
        valueStep = -1;

    // FIXME: This recurses to a possible depth of the length of the list.
    // That's not good -- we need to change this to an iterative algorithm.
    if (RenderListItem* previousItem = previousListItem(list, this))
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

bool RenderListItem::isEmpty() const
{
    return lastChild() == m_marker;
}

static RenderBlock* getParentOfFirstLineBox(RenderBlock* curr, RenderObject* marker)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;

    bool inQuirksMode = curr->document().inQuirksMode();
    for (RenderObject* currChild = firstChild; currChild; currChild = currChild->nextSibling()) {
        if (currChild == marker)
            continue;

        if (currChild->isInline() && (!currChild->isRenderInline() || curr->generatesLineBoxesForInlineChild(currChild)))
            return curr;

        if (currChild->isFloating() || currChild->isOutOfFlowPositioned())
            continue;

        if (currChild->isTable() || !currChild->isRenderBlock() || (currChild->isBox() && toRenderBox(currChild)->isWritingModeRoot()))
            break;

        if (curr->isListItem() && inQuirksMode && currChild->node() &&
            (currChild->node()->hasTagName(ulTag)|| currChild->node()->hasTagName(olTag)))
            break;

        RenderBlock* lineBox = getParentOfFirstLineBox(toRenderBlock(currChild), marker);
        if (lineBox)
            return lineBox;
    }

    return 0;
}

void RenderListItem::updateValue()
{
    if (!m_hasExplicitValue) {
        m_isValueUpToDate = false;
        if (m_marker)
            m_marker->setNeedsLayoutAndPrefWidthsRecalc();
    }
}

static RenderObject* firstNonMarkerChild(RenderBlock* parent)
{
    RenderObject* result = parent->firstChild();
    while (result && result->isListMarker())
        result = result->nextSibling();
    return result;
}

void RenderListItem::insertOrMoveMarkerRendererIfNeeded()
{
    // Sanity check the location of our marker.
    if (!m_marker)
        return;

    RenderElement* currentParent = m_marker->parent();
    RenderBlock* newParent = getParentOfFirstLineBox(this, m_marker.get());
    if (!newParent) {
        // If the marker is currently contained inside an anonymous box,
        // then we are the only item in that anonymous box (since no line box
        // parent was found). It's ok to just leave the marker where it is
        // in this case.
        if (currentParent && currentParent->isAnonymousBlock())
            return;
        if (multiColumnFlowThread())
            newParent = multiColumnFlowThread();
        else
            newParent = this;
    }

    if (newParent != currentParent) {
        // Removing and adding the marker can trigger repainting in
        // containers other than ourselves, so we need to disable LayoutState.
        LayoutStateDisabler layoutStateDisabler(&view());
        m_marker->removeFromParent();
        newParent->addChild(m_marker.get(), firstNonMarkerChild(newParent));
        m_marker->updateMarginsAndContent();
        // If current parent is an anonymous block that has lost all its children, destroy it.
        if (currentParent && currentParent->isAnonymousBlock() && !currentParent->firstChild() && !toRenderBlock(currentParent)->continuation())
            currentParent->destroy();
    }

}

void RenderListItem::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout()); 

    insertOrMoveMarkerRendererIfNeeded();
    RenderBlockFlow::layout();
}

void RenderListItem::addOverflowFromChildren()
{
    positionListMarker();
    RenderBlockFlow::addOverflowFromChildren();
}

void RenderListItem::computePreferredLogicalWidths()
{
#ifndef NDEBUG
    // FIXME: We shouldn't be modifying the tree in computePreferredLogicalWidths.
    // Instead, we should insert the marker soon after the tree construction.
    // This is similar case to RenderCounter::computePreferredLogicalWidths()
    // See https://bugs.webkit.org/show_bug.cgi?id=104829
    SetLayoutNeededForbiddenScope layoutForbiddenScope(this, false);
#endif
    insertOrMoveMarkerRendererIfNeeded();
    RenderBlockFlow::computePreferredLogicalWidths();
}

void RenderListItem::positionListMarker()
{
    if (m_marker && m_marker->parent()->isBox() && !m_marker->isInside() && m_marker->inlineBoxWrapper()) {
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
            LayoutUnit leftLineOffset = logicalLeftOffsetForLine(blockOffset, logicalLeftOffsetForLine(blockOffset, false), false);
            markerLogicalLeft = leftLineOffset - lineOffset - paddingStart() - borderStart() + m_marker->marginStart();
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
            LayoutUnit rightLineOffset = logicalRightOffsetForLine(blockOffset, logicalRightOffsetForLine(blockOffset, false), false);
            markerLogicalLeft = rightLineOffset - lineOffset + paddingStart() + borderStart() + m_marker->marginEnd();
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
            RenderBox* o = m_marker.get();
            bool propagateVisualOverflow = true;
            bool propagateLayoutOverflow = true;
            do {
                o = o->parentBox();
                if (o->hasOverflowClip())
                    propagateVisualOverflow = false;
                if (o->isRenderBlock()) {
                    if (propagateVisualOverflow)
                        toRenderBlock(o)->addVisualOverflow(markerRect);
                    if (propagateLayoutOverflow)
                        toRenderBlock(o)->addLayoutOverflow(markerRect);
                }
                if (o->hasOverflowClip())
                    propagateLayoutOverflow = false;
                if (o->hasSelfPaintingLayer())
                    propagateVisualOverflow = false;
                markerRect.moveBy(-o->location());
            } while (o != this && propagateVisualOverflow && propagateLayoutOverflow);
        }
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
    return nullAtom.string();
}

String RenderListItem::markerTextWithSuffix() const
{
    if (!m_marker)
        return String();

    // Append the suffix for the marker in the right place depending
    // on the direction of the text (right-to-left or left-to-right).

    const String& markerText = m_marker->text();
    const String markerSuffix = m_marker->suffix();
    StringBuilder result;

    if (!m_marker->style().isLeftToRightDirection())
        result.append(markerSuffix);

    result.append(markerText);

    if (m_marker->style().isLeftToRightDirection())
        result.append(markerSuffix);

    return result.toString();
}

void RenderListItem::explicitValueChanged()
{
    if (m_marker)
        m_marker->setNeedsLayoutAndPrefWidthsRecalc();
    Element* listNode = enclosingList(this);
    for (RenderListItem* item = this; item; item = nextListItem(listNode, item))
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

static RenderListItem* previousOrNextItem(bool isListReversed, Element* list, RenderListItem* item)
{
    return isListReversed ? previousListItem(list, item) : nextListItem(list, item);
}

void RenderListItem::updateListMarkerNumbers()
{
    Element* listNode = enclosingList(this);
    // The list node can be the shadow root which has no renderer.
    ASSERT(listNode);
    if (!listNode)
        return;

    bool isListReversed = false;
    HTMLOListElement* oListElement = (listNode && listNode->hasTagName(olTag)) ? toHTMLOListElement(listNode) : nullptr;
    if (oListElement) {
        oListElement->itemCountChanged();
        isListReversed = oListElement->isReversed();
    }
    for (RenderListItem* item = previousOrNextItem(isListReversed, listNode, this); item; item = previousOrNextItem(isListReversed, listNode, item)) {
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
