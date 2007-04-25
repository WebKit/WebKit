/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "RenderContainer.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "RenderCounter.h"
#include "RenderImage.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderTable.h"
#include "RenderTextFragment.h"
#include "RenderView.h"
#include "htmlediting.h"

namespace WebCore {

RenderContainer::RenderContainer(Node* node)
    : RenderBox(node)
    , m_firstChild(0)
    , m_lastChild(0)
{
}

RenderContainer::~RenderContainer()
{
}

void RenderContainer::destroy()
{
    destroyLeftoverChildren();
    RenderBox::destroy();
}

void RenderContainer::destroyLeftoverChildren()
{
    while (m_firstChild) {
        if (m_firstChild->isListMarker() || (m_firstChild->style()->styleType() == RenderStyle::FIRST_LETTER && !m_firstChild->isText()))
            m_firstChild->remove();  // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else {
        // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (m_firstChild->element())
                m_firstChild->element()->setRenderer(0);
            m_firstChild->destroy();
        }
    }
}

bool RenderContainer::canHaveChildren() const
{
    return true;
}

static void updateListMarkerNumbers(RenderObject* child)
{
    for (RenderObject* r = child; r; r = r->nextSibling())
        if (r->isListItem())
            static_cast<RenderListItem*>(r)->updateValue();
}

void RenderContainer::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    bool needsTable = false;

    if(!newChild->isText() && !newChild->isReplaced()) {
        switch(newChild->style()->display()) {
        case LIST_ITEM:
            updateListMarkerNumbers(beforeChild ? beforeChild : m_lastChild);
            break;
        case INLINE:
        case BLOCK:
        case INLINE_BLOCK:
        case RUN_IN:
        case COMPACT:
        case BOX:
        case INLINE_BOX:
        case TABLE:
        case INLINE_TABLE:
        case TABLE_COLUMN:
            break;
        case TABLE_COLUMN_GROUP:
        case TABLE_CAPTION:
        case TABLE_ROW_GROUP:
        case TABLE_HEADER_GROUP:
        case TABLE_FOOTER_GROUP:
            if (!isTable())
                needsTable = true;
            break;
        case TABLE_ROW:
            if (!isTableSection())
                needsTable = true;
            break;
        case TABLE_CELL:
            if (!isTableRow())
                needsTable = true;
            // I'm not 100% sure this is the best way to fix this, but without this
            // change we recurse infinitely when trying to render the CSS2 test page:
            // http://www.bath.ac.uk/%7Epy8ieh/internet/eviltests/htmlbodyheadrendering2.html.
            // See Radar 2925291.
            if (isTableCell() && !m_firstChild && !newChild->isTableCell())
                needsTable = false;
            break;
        case NONE:
            break;
        }
    }

    if (needsTable) {
        RenderTable *table;
        if(!beforeChild)
            beforeChild = m_lastChild;
        if(beforeChild && beforeChild->isAnonymous() && beforeChild->isTable())
            table = static_cast<RenderTable*>(beforeChild);
        else {
            table = new (renderArena()) RenderTable(document() /* is anonymous */);
            RenderStyle *newStyle = new (renderArena()) RenderStyle;
            newStyle->inheritFrom(style());
            newStyle->setDisplay(TABLE);
            table->setStyle(newStyle);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else {
        // just add it...
        insertChildNode(newChild, beforeChild);
    }
    
    if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE) {
        RefPtr<StringImpl> textToTransform = static_cast<RenderText*>(newChild)->originalText();
        if (textToTransform)
            static_cast<RenderText*>(newChild)->setText(textToTransform.release(), true);
    }
}

RenderObject* RenderContainer::removeChildNode(RenderObject* oldChild)
{
    ASSERT(oldChild->parent() == this);

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked).  We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed()) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        oldChild->repaint();
    }
        
    // If we have a line box wrapper, delete it.
    oldChild->deleteLineBoxWrapper();

    if (!documentBeingDestroyed()) {
        // if we remove visible child from an invisible parent, we don't know the layer visibility any more
        RenderLayer* layer = 0;
        if (m_style->visibility() != VISIBLE && oldChild->style()->visibility() == VISIBLE && !oldChild->hasLayer()) {
            layer = enclosingLayer();
            layer->dirtyVisibleContentStatus();
        }

         // Keep our layer hierarchy updated.
        if (oldChild->firstChild() || oldChild->hasLayer()) {
            if (!layer) layer = enclosingLayer();            
            oldChild->removeLayers(layer);
        }
        
        // If oldChild is the start or end of the selection, then clear the selection to
        // avoid problems of invalid pointers.
        // FIXME: The SelectionController should be responsible for this when it
        // is notified of DOM mutations.
        if (oldChild->isSelectionBorder())
            view()->clearSelection();

        // renumber ordered lists
        if (oldChild->isListItem())
            updateListMarkerNumbers(oldChild->nextSibling());
        
        if (oldChild->isPositioned() && childrenInline())
            dirtyLinesFromChangedChild(oldChild);
    }
    
    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_firstChild == oldChild)
        m_firstChild = oldChild->nextSibling();
    if (m_lastChild == oldChild)
        m_lastChild = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);

    return oldChild;
}

void RenderContainer::removeChild(RenderObject* oldChild)
{
    // We do this here instead of in removeChildNode, since the only extremely low-level uses of remove/appendChildNode
    // cannot affect the positioned object list, and the floating object list is irrelevant (since the list gets cleared on
    // layout anyway).
    oldChild->removeFromObjectLists();
    
    removeChildNode(oldChild);
}

RenderObject* RenderContainer::pseudoChild(RenderStyle::PseudoId type) const
{
    if (type == RenderStyle::BEFORE) {
        RenderObject* first = firstChild();
        while (first && first->isListMarker())
            first = first->nextSibling();
        return first;
    }
    if (type == RenderStyle::AFTER)
        return lastChild();

    ASSERT_NOT_REACHED();
    return 0;
}

void RenderContainer::updatePseudoChild(RenderStyle::PseudoId type)
{
    // If this is an anonymous wrapper, then parent applies its own pseudo style to it.
    if (parent() && parent()->createsAnonymousWrapper())
        return;
    updatePseudoChildForObject(type, this);
}

void RenderContainer::updatePseudoChildForObject(RenderStyle::PseudoId type, RenderObject* styledObject)
{
    // In CSS2, before/after pseudo-content cannot nest.  Check this first.
    if (style()->styleType() == RenderStyle::BEFORE || style()->styleType() == RenderStyle::AFTER)
        return;
    
    RenderStyle* pseudo = styledObject->getPseudoStyle(type);
    RenderObject* child = pseudoChild(type);

    // Whether or not we currently have generated content attached.
    bool oldContentPresent = child && (child->style()->styleType() == type);

    // Whether or not we now want generated content.  
    bool newContentWanted = pseudo && pseudo->display() != NONE;

    // For <q><p/></q>, if this object is the inline continuation of the <q>, we only want to generate
    // :after content and not :before content.
    if (newContentWanted && type == RenderStyle::BEFORE && isInlineContinuation())
        newContentWanted = false;

    // Similarly, if we're the beginning of a <q>, and there's an inline continuation for our object,
    // then we don't generate the :after content.
    if (newContentWanted && type == RenderStyle::AFTER && isRenderInline() && continuation())
        newContentWanted = false;
    
    // If we don't want generated content any longer, or if we have generated content, but it's no longer
    // identical to the new content data we want to build render objects for, then we nuke all
    // of the old generated content.
    if (!newContentWanted || (oldContentPresent && !child->style()->contentDataEquivalent(pseudo))) {
        // Nuke the child. 
        if (child && child->style()->styleType() == type) {
            oldContentPresent = false;
            child->destroy();
            child = (type == RenderStyle::BEFORE) ? m_firstChild : m_lastChild;
        }
    }

    // If we have no pseudo-style or if the pseudo's display type is NONE, then we
    // have no generated content and can now return.
    if (!newContentWanted)
        return;

    if (isInlineFlow() && !pseudo->isDisplayInlineType())
        // According to the CSS2 spec (the end of section 12.1), the only allowed
        // display values for the pseudo style are NONE and INLINE for inline flows.
        // FIXME: CSS2.1 lifted this restriction, but block display types will crash.
        // For now we at least relax the restriction to allow all inline types like inline-block
        // and inline-table.
        pseudo->setDisplay(INLINE);

    if (oldContentPresent) {
        if (child && child->style()->styleType() == type) {
            // We have generated content present still.  We want to walk this content and update our
            // style information with the new pseudo style.
            child->setStyle(pseudo);

            // Note that if we ever support additional types of generated content (which should be way off
            // in the future), this code will need to be patched.
            for (RenderObject* genChild = child->firstChild(); genChild; genChild = genChild->nextSibling()) {
                if (genChild->isText())
                    // Generated text content is a child whose style also needs to be set to the pseudo style.
                    genChild->setStyle(pseudo);
                else {
                    // Images get an empty style that inherits from the pseudo.
                    RenderStyle* style = new (renderArena()) RenderStyle;
                    style->inheritFrom(pseudo);
                    genChild->setStyle(style);
                }
            }
        }
        return; // We've updated the generated content. That's all we needed to do.
    }
    
    RenderObject* insertBefore = (type == RenderStyle::BEFORE) ? child : 0;

    // Generated content consists of a single container that houses multiple children (specified
    // by the content property).  This pseudo container gets the pseudo style set on it.
    RenderObject* pseudoContainer = 0;
    
    // Walk our list of generated content and create render objects for each.
    for (const ContentData* content = pseudo->contentData(); content; content = content->m_next) {
        RenderObject* renderer = 0;
        switch (content->m_type) {
            case CONTENT_NONE:
                break;
            case CONTENT_TEXT:
                renderer = new (renderArena()) RenderTextFragment(document() /* anonymous object */, content->m_content.m_text);
                renderer->setStyle(pseudo);
                break;
            case CONTENT_OBJECT:
                if (CachedResource* resource = content->m_content.m_object)
                    if (resource->type() == CachedResource::ImageResource) {
                        RenderImage* image = new (renderArena()) RenderImage(document()); // anonymous object
                        RenderStyle* style = new (renderArena()) RenderStyle;
                        style->inheritFrom(pseudo);
                        image->setStyle(style);
                        image->setCachedImage(static_cast<CachedImage*>(resource));
                        image->setIsAnonymousImage(true);
                        renderer = image;
                    }
                break;
            case CONTENT_COUNTER:
                renderer = new (renderArena()) RenderCounter(document(), *content->m_content.m_counter);
                renderer->setStyle(pseudo);
                break;
        }

        if (renderer) {
            if (!pseudoContainer)
                pseudoContainer = RenderFlow::createAnonymousFlow(document(), pseudo); // anonymous box
            pseudoContainer->addChild(renderer);
        }
    }

    // Add the pseudo after we've installed all our content so that addChild will be able to find the text
    // inside the inline for e.g., first-letter styling.
    if (pseudoContainer)
        addChild(pseudoContainer, insertBefore);
}


void RenderContainer::appendChildNode(RenderObject* newChild)
{
    ASSERT(newChild->parent() == 0);
    ASSERT(!isBlockFlow() || (!newChild->isTableSection() && !newChild->isTableRow() && !newChild->isTableCell()));

    newChild->setParent(this);
    RenderObject* lChild = m_lastChild;

    if (lChild) {
        newChild->setPreviousSibling(lChild);
        lChild->setNextSibling(newChild);
    } else
        m_firstChild = newChild;

    m_lastChild = newChild;
    
    // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    RenderLayer* layer = 0;
    if (newChild->firstChild() || newChild->layer()) {
        layer = enclosingLayer();
        newChild->addLayers(layer, newChild);
    }

    // if the new child is visible but this object was not, tell the layer it has some visible content
    // that needs to be drawn and layer visibility optimization can't be used
    if (style()->visibility() != VISIBLE && newChild->style()->visibility() == VISIBLE && !newChild->layer()) {
        if (!layer)
            layer = enclosingLayer();
        if (layer)
            layer->setHasVisibleContent(true);
    }
    
    newChild->setNeedsLayoutAndPrefWidthsRecalc(); // Goes up the containing block hierarchy.
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (!newChild->isFloatingOrPositioned() && childrenInline())
        dirtyLinesFromChangedChild(newChild);
    
    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

void RenderContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild)
{
    if (!beforeChild) {
        appendChildNode(child);
        return;
    }

    ASSERT(!child->parent());
    while (beforeChild->parent() != this && beforeChild->parent()->isAnonymousBlock())
        beforeChild = beforeChild->parent();
    ASSERT(beforeChild->parent() == this);

    ASSERT(!isBlockFlow() || (!child->isTableSection() && !child->isTableRow() && !child->isTableCell()));

    if (beforeChild == m_firstChild)
        m_firstChild = child;

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if(prev) prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(this);
    
    // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    RenderLayer* layer = 0;
    if (child->firstChild() || child->layer()) {
        layer = enclosingLayer();
        child->addLayers(layer, child);
    }

    // if the new child is visible but this object was not, tell the layer it has some visible content
    // that needs to be drawn and layer visibility optimization can't be used
    if (style()->visibility() != VISIBLE && child->style()->visibility() == VISIBLE && !child->layer()) {
        if (!layer)
            layer = enclosingLayer();
        if (layer)
            layer->setHasVisibleContent(true);
    }

    child->setNeedsLayoutAndPrefWidthsRecalc();
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (!child->isFloating() && childrenInline())
        dirtyLinesFromChangedChild(child);
    
    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->childrenChanged(this);
}

void RenderContainer::layout()
{
    ASSERT(needsLayout());
    ASSERT(!prefWidthsDirty());

    RenderObject* child = m_firstChild;
    while (child) {
        child->layoutIfNeeded();
        ASSERT(child->isRenderInline() || !child->needsLayout());
        child = child->nextSibling();
    }
    setNeedsLayout(false);
}

void RenderContainer::removeLeftoverAnonymousBoxes()
{
    // we have to go over all child nodes and remove anonymous boxes, that do _not_
    // have inline children to keep the tree flat
    RenderObject* child = m_firstChild;
    while( child ) {
        RenderObject* next = child->nextSibling();
        
        if ( child->isRenderBlock() && child->isAnonymousBlock() && !child->continuation() && !child->childrenInline() && !child->isTableCell() ) {
            RenderObject* firstAnChild = child->firstChild();
            RenderObject* lastAnChild = child->lastChild();
            if ( firstAnChild ) {
                RenderObject* o = firstAnChild;
                while( o ) {
                    o->setParent( this );
                    o = o->nextSibling();
                }
                firstAnChild->setPreviousSibling( child->previousSibling() );
                lastAnChild->setNextSibling( child->nextSibling() );
                if ( child->previousSibling() )
                    child->previousSibling()->setNextSibling( firstAnChild );
                if ( child->nextSibling() )
                    child->nextSibling()->setPreviousSibling( lastAnChild );
            } else {
                if ( child->previousSibling() )
                    child->previousSibling()->setNextSibling( child->nextSibling() );
                if ( child->nextSibling() )
                    child->nextSibling()->setPreviousSibling( child->previousSibling() );
                
            }
            if (child == m_firstChild)
                m_firstChild = firstAnChild;
            if (child == m_lastChild)
                m_lastChild = lastAnChild;
            child->setParent( 0 );
            child->setPreviousSibling( 0 );
            child->setNextSibling( 0 );
            if ( !child->isText() ) {
                RenderContainer *c = static_cast<RenderContainer*>(child);
                c->m_firstChild = 0;
                c->m_next = 0;
            }
            child->destroy();
        }
        child = next;
    }

    if (parent()) // && isAnonymousBlock() && !continuation() && !childrenInline() && !isTableCell())
        parent()->removeLeftoverAnonymousBoxes();
}

VisiblePosition RenderContainer::positionForCoordinates(int x, int y)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!m_firstChild)
        return VisiblePosition(element(), 0, DOWNSTREAM);
        
    if (isTable() && element()) {
        int right = contentWidth() + borderRight() + paddingRight() + borderLeft() + paddingLeft();
        int bottom = contentHeight() + borderTop() + paddingTop() + borderBottom() + paddingBottom();
        
        if (x < 0 || x > right || y < 0 || y > bottom) {
            if (x <= right / 2)
                return VisiblePosition(Position(element(), 0));
            else
                return VisiblePosition(Position(element(), maxDeepOffset(element())));
        }
    }

    // Pass off to the closest child.
    int minDist = INT_MAX;
    RenderObject* closestRenderer = 0;
    int newX = x;
    int newY = y;
    if (isTableRow()) {
        newX += xPos();
        newY += yPos();
    }
    for (RenderObject* renderer = m_firstChild; renderer; renderer = renderer->nextSibling()) {
        if (!renderer->firstChild() && !renderer->isInline() && !renderer->isBlockFlow() 
            || renderer->style()->visibility() != VISIBLE)
            continue;
        
        int top = borderTop() + paddingTop() + isTableRow() ? 0 : renderer->xPos();
        int bottom = top + renderer->contentHeight();
        int left = borderLeft() + paddingLeft() + isTableRow() ? 0 : renderer->yPos();
        int right = left + renderer->contentWidth();
        
        if (x <= right && x >= left && y <= top && y >= bottom) {
            if (renderer->isTableRow())
                return renderer->positionForCoordinates(x + newX - renderer->xPos(), y + newY - renderer->yPos());
            return renderer->positionForCoordinates(x - renderer->xPos(), y - renderer->yPos());
        }

        // Find the distance from (x, y) to the box.  Split the space around the box into 8 pieces
        // and use a different compare depending on which piece (x, y) is in.
        IntPoint cmp;
        if (x > right) {
            if (y < top)
                cmp = IntPoint(right, top);
            else if (y > bottom)
                cmp = IntPoint(right, bottom);
            else
                cmp = IntPoint(right, y);
        } else if (x < left) {
            if (y < top)
                cmp = IntPoint(left, top);
            else if (y > bottom)
                cmp = IntPoint(left, bottom);
            else
                cmp = IntPoint(left, y);
        } else {
            if (y < top)
                cmp = IntPoint(x, top);
            else
                cmp = IntPoint(x, bottom);
        }
        
        int x1minusx2 = cmp.x() - x;
        int y1minusy2 = cmp.y() - y;
        
        int dist = x1minusx2 * x1minusx2 + y1minusy2 * y1minusy2;
        if (dist < minDist) {
            closestRenderer = renderer;
            minDist = dist;
        }
    }
    
    if (closestRenderer)
        return closestRenderer->positionForCoordinates(newX - closestRenderer->xPos(), newY - closestRenderer->yPos());
    
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

void RenderContainer::addLineBoxRects(Vector<IntRect>& rects, unsigned start, unsigned end)
{
    if (!m_firstChild && (isInline() || isAnonymousBlock())) {
        int x, y;
        absolutePositionForContent(x, y);
        absoluteRects(rects, x, y);
        return;
    }

    if (!m_firstChild)
        return;

    unsigned offset = start;
    for (RenderObject* child = childAt(start); child && offset < end; child = child->nextSibling(), ++offset) {
        if (child->isText() || child->isInline() || child->isAnonymousBlock()) {
            int x, y;
            child->absolutePositionForContent(x, y);
            child->absoluteRects(rects, x, y);
        }
    }
}

#undef DEBUG_LAYOUT

} // namespace WebCore
