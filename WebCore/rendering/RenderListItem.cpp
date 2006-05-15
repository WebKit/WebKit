/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#include "config.h"
#include "RenderListItem.h"

#include "CachedImage.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "RenderListMarker.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderListItem::RenderListItem(Node* node)
    : RenderBlock(node)
    , m_predefVal(-1)
    , m_marker(0)
    , m_notInList(false)
    , m_value(-1)
{
    // init RenderObject attributes
    setInline(false); // our object is not Inline
}

void RenderListItem::setStyle(RenderStyle* _style)
{
    RenderBlock::setStyle(_style);

    if (style()->listStyleType() != LNONE ||
        (style()->listStyleImage() && !style()->listStyleImage()->isErrorImage())) {
        RenderStyle *newStyle = new (renderArena()) RenderStyle();
        newStyle->ref();
        // The marker always inherits from the list item, regardless of where it might end
        // up (e.g., in some deeply nested line box).  See CSS3 spec.
        newStyle->inheritFrom(style()); 
        if (!m_marker) {
            m_marker = new (renderArena()) RenderListMarker(document());
            m_marker->setStyle(newStyle);
            m_marker->setListItem(this);
        } else
            m_marker->setStyle(newStyle);
        newStyle->deref(renderArena());
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

static Node* enclosingList(Node* node)
{
    for (Node* n = node->parentNode(); n; n = n->parentNode())
        if (n->hasTagName(ulTag) || n->hasTagName(olTag))
            return n;
    return 0;
}

static RenderListItem* previousListItem(Node* list, RenderListItem* item)
{
    if (!list)
        return 0;
    for (Node* n = item->node()->traversePreviousNode(); n != list; n = n->traversePreviousNode()) {
        RenderObject* o = n->renderer();
        if (o && o->isListItem()) {
            Node* otherList = enclosingList(n);
            // This item is part of our current list, so it's what we're looking for.
            if (list == otherList)
                return static_cast<RenderListItem*>(o);
            // We found ourself inside another list; lets skip the rest of it.
            if (otherList)
                n = otherList;
        }
    }
    return 0;
}

void RenderListItem::calcValue()
{
    if (m_predefVal != -1)
        m_value = m_predefVal;
    else {
        Node* list = enclosingList(node());
        RenderListItem* item = previousListItem(list, this);
        if (item) {
            // FIXME: This recurses to a possible depth of the length of the list.
            // That's not good -- we need to change this to an iterative algorithm.
            if (item->value() == -1)
                item->calcValue();
            m_value = item->value() + 1;
        } else if (list && list->hasTagName(olTag))
            m_value = static_cast<HTMLOListElement*>(list)->start();
        else
            m_value = 1;
    }
}

bool RenderListItem::isEmpty() const
{
    return lastChild() == m_marker;
}

static RenderObject* getParentOfFirstLineBox(RenderObject* curr, RenderObject* marker)
{
    RenderObject* firstChild = curr->firstChild();
    if (!firstChild)
        return 0;
        
    for (RenderObject* currChild = firstChild; currChild; currChild = currChild->nextSibling()) {
        if (currChild == marker)
            continue;
            
        if (currChild->isInline())
            return curr;
        
        if (currChild->isFloating() || currChild->isPositioned())
            continue;
            
        if (currChild->isTable() || !currChild->isRenderBlock())
            break;
        
        if (currChild->style()->htmlHacks() && currChild->element() &&
            (currChild->element()->hasTagName(ulTag)|| currChild->element()->hasTagName(olTag)))
            break;
            
        RenderObject* lineBox = getParentOfFirstLineBox(currChild, marker);
        if (lineBox)
            return lineBox;
    }
    
    return 0;
}

void RenderListItem::resetValue()
{
    m_value = -1;
    if (m_marker)
        m_marker->setNeedsLayoutAndMinMaxRecalc();
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
        
        if (markerPar != lineBoxParent || !m_marker->minMaxKnown()) {
            if (markerPar)
                markerPar->removeChild(m_marker);
            if (!lineBoxParent)
                lineBoxParent = this;
            lineBoxParent->addChild(m_marker, lineBoxParent->firstChild());
            if (!m_marker->minMaxKnown())
                m_marker->calcMinMaxWidth();
            recalcMinMaxWidths();
        }
    }
}

void RenderListItem::calcMinMaxWidth()
{
    // Make sure our marker is in the correct location.
    updateMarkerLocation();
    if (!minMaxKnown())
        RenderBlock::calcMinMaxWidth();
}

void RenderListItem::layout()
{
    KHTMLAssert(needsLayout());
    KHTMLAssert(minMaxKnown());
    
    updateMarkerLocation();    
    RenderBlock::layout();
}

void RenderListItem::positionListMarker()
{
    if (m_marker && !m_marker->isInside()) {
        int markerOldX = m_marker->xPos();
        int yOffset = 0;
        int xOffset = 0;
        for (RenderObject* o = m_marker->parent(); o != this; o = o->parent()) {
            yOffset += o->yPos();
            xOffset += o->xPos();
        }
        
        RootInlineBox* root = m_marker->inlineBoxWrapper()->root();
        if (style()->direction() == LTR) {
            int leftLineOffset = leftRelOffset(yOffset, leftOffset(yOffset));
            int markerXPos = leftLineOffset - xOffset - paddingLeft() - borderLeft() + m_marker->marginLeft();
            m_marker->inlineBoxWrapper()->adjustPosition(markerXPos - markerOldX, 0);
            if (markerXPos < root->leftOverflow()) {
                root->setHorizontalOverflowPositions(markerXPos, root->rightOverflow());
                m_overflowLeft = min(markerXPos, m_overflowLeft);
            }
        } else {
            int rightLineOffset = rightRelOffset(yOffset, rightOffset(yOffset));
            int markerXPos = rightLineOffset - xOffset + paddingRight() + borderRight() + m_marker->marginLeft();
            m_marker->inlineBoxWrapper()->adjustPosition(markerXPos - markerOldX, 0);
            if (markerXPos + m_marker->width() > root->rightOverflow()) {
                root->setHorizontalOverflowPositions(root->leftOverflow(), markerXPos + m_marker->width());
                m_overflowWidth = max(markerXPos + m_marker->width(), m_overflowLeft);
            }
        }
    }
}

void RenderListItem::paint(PaintInfo& i, int _tx, int _ty)
{
    if (!m_height)
        return;

    RenderBlock::paint(i, _tx, _ty);
}

DeprecatedString RenderListItem::markerStringValue()
{
    return m_marker ? m_marker->text() : "";
}

}
