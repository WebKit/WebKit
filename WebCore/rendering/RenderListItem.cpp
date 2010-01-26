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

#include "CachedImage.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "RenderListMarker.h"
#include "RenderView.h"
#include <wtf/StdLibExtras.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderListItem::RenderListItem(Node* node)
    : RenderBlock(node)
    , m_marker(0)
    , m_hasExplicitValue(false)
    , m_isValueUpToDate(false)
    , m_notInList(false)
{
    setInline(false);
}

void RenderListItem::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    RenderBlock::styleDidChange(diff, oldStyle);

    if (style()->listStyleType() != NoneListStyle
        || (style()->listStyleImage() && !style()->listStyleImage()->errorOccurred())) {
        RefPtr<RenderStyle> newStyle = RenderStyle::create();
        // The marker always inherits from the list item, regardless of where it might end
        // up (e.g., in some deeply nested line box). See CSS3 spec.
        newStyle->inheritFrom(style()); 
        if (!m_marker)
            m_marker = new (renderArena()) RenderListMarker(this);
        m_marker->setStyle(newStyle.release());
    } else if (m_marker) {
        m_marker->destroy();
        m_marker = 0;
    }
}

void RenderListItem::destroy()
{    
    if (m_marker) {
        m_marker->destroy();
        m_marker = 0;
    }
    RenderBlock::destroy();
}

static bool isList(Node* node)
{
    return (node->hasTagName(ulTag) || node->hasTagName(olTag));
}

static Node* enclosingList(Node* node)
{
    Node* parent = node->parentNode();
    for (Node* n = parent; n; n = n->parentNode())
        if (isList(n))
            return n;
    // If there's no actual <ul> or <ol> list element, then our parent acts as
    // our list for purposes of determining what other list items should be
    // numbered as part of the same list.
    return parent;
}

static Node* enclosingList(const RenderObject* renderer)
{
    Node* node = renderer->node();
    if (node)
        return enclosingList(node);

    renderer = renderer->parent();
    while (renderer && !renderer->node())
        renderer = renderer->parent();

    node = renderer->node();
    if (isList(node))
        return node;

    return enclosingList(node);
}

static RenderListItem* previousListItem(Node* list, const RenderListItem* item)
{
    for (RenderObject* renderer = item->previousInPreOrder(); renderer != list->renderer(); renderer = renderer->previousInPreOrder()) {
        if (!renderer->isListItem())
            continue;
        Node* otherList = enclosingList(renderer);
        // This item is part of our current list, so it's what we're looking for.
        if (list == otherList)
            return toRenderListItem(renderer);
        // We found ourself inside another list; lets skip the rest of it.
        // Use nextInPreOrder() here because the other list itself may actually
        // be a list item itself. We need to examine it, so we do this to counteract
        // the previousInPreOrder() that will be done by the loop.
        if (otherList)
            renderer = otherList->renderer()->nextInPreOrder();
    }
    return 0;
}

inline int RenderListItem::calcValue() const
{
    if (m_hasExplicitValue)
        return m_explicitValue;
    Node* list = enclosingList(this);
    // FIXME: This recurses to a possible depth of the length of the list.
    // That's not good -- we need to change this to an iterative algorithm.
    if (RenderListItem* previousItem = previousListItem(list, this))
        return previousItem->value() + 1;
    if (list && list->hasTagName(olTag))
        return static_cast<HTMLOListElement*>(list)->start();
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

static RenderObject* getParentOfFirstLineBox(RenderBlock* curr, RenderObject* marker)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;

    for (RenderObject* currChild = firstChild; currChild; currChild = currChild->nextSibling()) {
        if (currChild == marker)
            continue;

        if (currChild->isInline() && (!currChild->isRenderInline() || curr->generatesLineBoxesForInlineChild(currChild)))
            return curr;

        if (currChild->isFloating() || currChild->isPositioned())
            continue;

        if (currChild->isTable() || !currChild->isRenderBlock())
            break;

        if (curr->isListItem() && currChild->style()->htmlHacks() && currChild->node() &&
            (currChild->node()->hasTagName(ulTag)|| currChild->node()->hasTagName(olTag)))
            break;

        RenderObject* lineBox = getParentOfFirstLineBox(toRenderBlock(currChild), marker);
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

static RenderObject* firstNonMarkerChild(RenderObject* parent)
{
    RenderObject* result = parent->firstChild();
    while (result && result->isListMarker())
        result = result->nextSibling();
    return result;
}

void RenderListItem::updateMarkerLocation()
{
    // Sanity check the location of our marker.
    if (m_marker) {
        RenderObject* markerPar = m_marker->parent();
        RenderObject* lineBoxParent = getParentOfFirstLineBox(this, m_marker);
        if (!lineBoxParent) {
            // If the marker is currently contained inside an anonymous box,
            // then we are the only item in that anonymous box (since no line box
            // parent was found).  It's ok to just leave the marker where it is
            // in this case.
            if (markerPar && markerPar->isAnonymousBlock())
                lineBoxParent = markerPar;
            else
                lineBoxParent = this;
        }

        if (markerPar != lineBoxParent || m_marker->prefWidthsDirty()) {
            // Removing and adding the marker can trigger repainting in
            // containers other than ourselves, so we need to disable LayoutState.
            view()->disableLayoutState();
            updateFirstLetter();
            m_marker->remove();
            if (!lineBoxParent)
                lineBoxParent = this;
            lineBoxParent->addChild(m_marker, firstNonMarkerChild(lineBoxParent));
            if (m_marker->prefWidthsDirty())
                m_marker->calcPrefWidths();
            view()->enableLayoutState();
        }
    }
}

void RenderListItem::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());
    
    updateMarkerLocation();

    RenderBlock::calcPrefWidths();
}

void RenderListItem::layout()
{
    ASSERT(needsLayout()); 

    updateMarkerLocation();    
    RenderBlock::layout();
}

void RenderListItem::positionListMarker()
{
    if (m_marker && !m_marker->isInside() && m_marker->inlineBoxWrapper()) {
        int markerOldX = m_marker->x();
        int yOffset = 0;
        int xOffset = 0;
        for (RenderBox* o = m_marker->parentBox(); o != this; o = o->parentBox()) {
            yOffset += o->y();
            xOffset += o->x();
        }

        bool adjustOverflow = false;
        int markerXPos;
        RootInlineBox* root = m_marker->inlineBoxWrapper()->root();

        // FIXME: Inline flows in the line box hierarchy that have self-painting layers should act as cutoff points
        // and really shouldn't keep propagating overflow up.  This won't really break anything other than repainting
        // not being as tight as it could be though.
        if (style()->direction() == LTR) {
            int leftLineOffset = leftRelOffset(yOffset, leftOffset(yOffset, false), false);
            markerXPos = leftLineOffset - xOffset - paddingLeft() - borderLeft() + m_marker->marginLeft();
            m_marker->inlineBoxWrapper()->adjustPosition(markerXPos - markerOldX, 0);
            for (InlineFlowBox* box = m_marker->inlineBoxWrapper()->parent(); box; box = box->parent()) {
                if (markerXPos < box->leftLayoutOverflow()) {
                    box->setHorizontalOverflowPositions(markerXPos, box->rightLayoutOverflow(), box->leftVisualOverflow(), box->rightVisualOverflow());
                    if (box == root)
                        adjustOverflow = true;
                }
            }
        } else {
            int rightLineOffset = rightRelOffset(yOffset, rightOffset(yOffset, false), false);
            markerXPos = rightLineOffset - xOffset + paddingRight() + borderRight() + m_marker->marginLeft();
            m_marker->inlineBoxWrapper()->adjustPosition(markerXPos - markerOldX, 0);
            for (InlineFlowBox* box = m_marker->inlineBoxWrapper()->parent(); box; box = box->parent()) {
                if (markerXPos + m_marker->width() > box->rightLayoutOverflow()) {
                    box->setHorizontalOverflowPositions(box->leftLayoutOverflow(), markerXPos + m_marker->width(), box->leftVisualOverflow(), box->rightVisualOverflow());
                    if (box == root)
                        adjustOverflow = true;
                }
            }
        }

        if (adjustOverflow) {
            IntRect markerRect(markerXPos + xOffset, yOffset, m_marker->width(), m_marker->height());
            RenderBox* o = m_marker;
            do {
                o = o->parentBox();
                if (o->isRenderBlock())
                    toRenderBlock(o)->addLayoutOverflow(markerRect);
                markerRect.move(-o->x(), -o->y());
            } while (o != this && !o->hasSelfPaintingLayer());
        }
    }
}

void RenderListItem::paint(PaintInfo& paintInfo, int tx, int ty)
{
    if (!height())
        return;

    RenderBlock::paint(paintInfo, tx, ty);
}

const String& RenderListItem::markerText() const
{
    if (m_marker)
        return m_marker->text();
    DEFINE_STATIC_LOCAL(String, staticNullString, ());
    return staticNullString;
}

void RenderListItem::explicitValueChanged()
{
    if (m_marker)
        m_marker->setNeedsLayoutAndPrefWidthsRecalc();
    Node* listNode = enclosingList(node());
    RenderObject* listRenderer = 0;
    if (listNode)
        listRenderer = listNode->renderer();
    for (RenderObject* renderer = this; renderer; renderer = renderer->nextInPreOrder(listRenderer))
        if (renderer->isListItem()) {
            RenderListItem* item = toRenderListItem(renderer);
            if (!item->m_hasExplicitValue) {
                item->m_isValueUpToDate = false;
                if (RenderListMarker* marker = item->m_marker)
                    marker->setNeedsLayoutAndPrefWidthsRecalc();
            }
        }
}

void RenderListItem::setExplicitValue(int value)
{
    ASSERT(node());

    if (m_hasExplicitValue && m_explicitValue == value)
        return;
    m_explicitValue = value;
    m_value = value;
    m_hasExplicitValue = true;
    explicitValueChanged();
}

void RenderListItem::clearExplicitValue()
{
    ASSERT(node());

    if (!m_hasExplicitValue)
        return;
    m_hasExplicitValue = false;
    m_isValueUpToDate = false;
    explicitValueChanged();
}

} // namespace WebCore
