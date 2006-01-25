/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderTable.h"
#include "RenderTextFragment.h"
#include "render_image.h"
#include "render_canvas.h"
#include "DocumentImpl.h"
#include "xml/dom_position.h"
#include "visible_position.h"

#include <assert.h>

// For accessibility
#include "KWQAccObjectCache.h" 

using DOM::Position;
namespace WebCore {

RenderContainer::RenderContainer(DOM::NodeImpl* node)
    : RenderBox(node)
{
    m_first = 0;
    m_last = 0;
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
    while (m_first) {
        if (m_first->isListMarker())
            m_first->remove();  // List markers are owned by their enclosing list and so don't get destroyed by this container.
        else {
        // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (m_first->element())
                m_first->element()->setRenderer(0);
            m_first->destroy();
        }
    }
}

bool RenderContainer::canHaveChildren() const
{
    return true;
}

void RenderContainer::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << this << ": " <<  renderName() << "(RenderObject)::addChild( " << newChild << ": " <<
        newChild->renderName() << ", " << (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
#endif

    bool needsTable = false;

    if(!newChild->isText() && !newChild->isReplaced()) {
        switch(newChild->style()->display()) {
        case INLINE:
        case BLOCK:
        case INLINE_BLOCK:
        case LIST_ITEM:
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

            //kdDebug( 6040 ) << "adding section" << endl;
            if ( !isTable() )
                needsTable = true;
            break;
        case TABLE_ROW:
            //kdDebug( 6040 ) << "adding row" << endl;
            if ( !isTableSection() )
                needsTable = true;
            break;
        case TABLE_CELL:
            //kdDebug( 6040 ) << "adding cell" << endl;
            if ( !isTableRow() )
                needsTable = true;
            // I'm not 100% sure this is the best way to fix this, but without this
            // change we recurse infinitely when trying to render the CSS2 test page:
            // http://www.bath.ac.uk/%7Epy8ieh/internet/eviltests/htmlbodyheadrendering2.html.
            // See Radar 2925291.
            if ( isTableCell() && !firstChild() && !newChild->isTableCell() )
                needsTable = false;
            break;
        case NONE:
            break;
        }
    }

    if ( needsTable ) {
        RenderTable *table;
        if( !beforeChild )
            beforeChild = lastChild();
        if( beforeChild && beforeChild->isAnonymous() && beforeChild->isTable() )
            table = static_cast<RenderTable *>(beforeChild);
        else {
            //kdDebug( 6040 ) << "creating anonymous table" << endl;
            table = new (renderArena()) RenderTable(document() /* is anonymous */);
            RenderStyle *newStyle = new (renderArena()) RenderStyle();
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
}

RenderObject* RenderContainer::removeChildNode(RenderObject* oldChild)
{
    KHTMLAssert(oldChild->parent() == this);

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked).  We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed()) {
        oldChild->setNeedsLayoutAndMinMaxRecalc();
        oldChild->repaint();
        
        // Keep our layer hierarchy updated.
        oldChild->removeLayers(enclosingLayer());
    
        // if oldChild is the start or end of the selection, then clear the selection to
        // avoid problems of invalid pointers
    
        // ### This is not the "proper" solution... ideally the selection should be maintained
        // based on DOM Nodes and a Range, which gets adjusted appropriately when nodes are
        // deleted/inserted near etc. But this at least prevents crashes caused when the start
        // or end of the selection is deleted and then accessed when the user next selects
        // something.
    
        if (oldChild->isSelectionBorder())
            canvas()->clearSelection();
    }
    
    // remove the child
    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_first == oldChild)
        m_first = oldChild->nextSibling();
    if (m_last == oldChild)
        m_last = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

#if __APPLE__
    if (KWQAccObjectCache::accessibilityEnabled())
        document()->getAccObjectCache()->childrenChanged(this);
#endif

    return oldChild;
}

void RenderContainer::removeChild(RenderObject *oldChild)
{
    removeChildNode(oldChild);
}

void RenderContainer::updatePseudoChild(RenderStyle::PseudoId type, RenderObject* child)
{
    // In CSS2, before/after pseudo-content cannot nest.  Check this first.
    if (style()->styleType() == RenderStyle::BEFORE || style()->styleType() == RenderStyle::AFTER)
        return;
    
    RenderStyle* pseudo = getPseudoStyle(type);

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
    if (!newContentWanted ||
        (oldContentPresent && !child->style()->contentDataEquivalent(pseudo))) {
        // Nuke the child. 
        if (child && child->style()->styleType() == type) {
            oldContentPresent = false;
            child->destroy();
            child = (type == RenderStyle::BEFORE) ? firstChild() : lastChild();
        }
    }

    // If we have no pseudo-style or if the pseudo's display type is NONE, then we
    // have no generated content and can now return.
    if (!newContentWanted)
        return;

    if (isInlineFlow() && pseudo->display() != INLINE)
        // According to the CSS2 spec (the end of section 12.1), the only allowed
        // display values for the pseudo style are NONE and INLINE.  Since we already
        // determined that the pseudo is not display NONE, any display other than
        // inline should be mutated to INLINE.
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
                    // Generated text content is a child whose style also needs to be set to the pseudo
                    // style.
                    genChild->setStyle(pseudo);
                else {
                    // Images get an empty style that inherits from the pseudo.
                    RenderStyle* style = new (renderArena()) RenderStyle();
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
    
    // Now walk our list of generated content and create render objects for every type
    // we encounter.
    for (ContentData* contentData = pseudo->contentData();
         contentData; contentData = contentData->_nextContent) {
        if (!pseudoContainer)
            pseudoContainer = RenderFlow::createAnonymousFlow(document(), pseudo); /* anonymous box */
        
        if (contentData->contentType() == CONTENT_TEXT)
        {
            RenderText* t = new (renderArena()) RenderTextFragment(document() /*anonymous object */, contentData->contentText());
            t->setStyle(pseudo);
            pseudoContainer->addChild(t);
        }
        else if (contentData->contentType() == CONTENT_OBJECT)
        {
            RenderImage* img = new (renderArena()) RenderImage(document()); /* Anonymous object */
            RenderStyle* style = new (renderArena()) RenderStyle();
            style->inheritFrom(pseudo);
            img->setStyle(style);
            img->setContentObject(contentData->contentObject());
            pseudoContainer->addChild(img);
        }
    }

    if (pseudoContainer) {
        // Add the pseudo after we've installed all our content, so that addChild will be able to find the text
        // inside the inline for e.g., first-letter styling.
        addChild(pseudoContainer, insertBefore);
    }
}


void RenderContainer::appendChildNode(RenderObject* newChild)
{
    KHTMLAssert(newChild->parent() == 0);

    newChild->setParent(this);
    RenderObject* lChild = lastChild();

    if(lChild)
    {
        newChild->setPreviousSibling(lChild);
        lChild->setNextSibling(newChild);
    }
    else
        setFirstChild(newChild);

    setLastChild(newChild);
    
    // Keep our layer hierarchy updated.  Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    if (newChild->firstChild() || newChild->layer()) {
        RenderLayer* layer = enclosingLayer();
        newChild->addLayers(layer, newChild);
    }
    
    newChild->setNeedsLayoutAndMinMaxRecalc(); // Goes up the containing block hierarchy.
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (!newChild->isFloatingOrPositioned() && childrenInline())
        dirtyLinesFromChangedChild(newChild);
    
#if __APPLE__
    if (KWQAccObjectCache::accessibilityEnabled())
        document()->getAccObjectCache()->childrenChanged(this);
#endif
}

void RenderContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild)
{
    if(!beforeChild) {
        appendChildNode(child);
        return;
    }

    KHTMLAssert(!child->parent());
    while ( beforeChild->parent() != this && beforeChild->parent()->isAnonymousBlock() )
	beforeChild = beforeChild->parent();
    KHTMLAssert(beforeChild->parent() == this);

    if(beforeChild == firstChild())
        setFirstChild(child);

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if(prev) prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(this);
    
    // Keep our layer hierarchy updated.
    RenderLayer* layer = enclosingLayer();
    child->addLayers(layer, child);

    child->setNeedsLayoutAndMinMaxRecalc();
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.
    
    if (!child->isFloatingOrPositioned() && childrenInline())
        dirtyLinesFromChangedChild(child);
    
#if __APPLE__
    if (KWQAccObjectCache::accessibilityEnabled())
        document()->getAccObjectCache()->childrenChanged(this);
#endif
}

void RenderContainer::layout()
{
    KHTMLAssert( needsLayout() );
    KHTMLAssert( minMaxKnown() );

    RenderObject *child = firstChild();
    while( child ) {
        child->layoutIfNeeded();
        child = child->nextSibling();
    }
    setNeedsLayout(false);
}

void RenderContainer::removeLeftoverAnonymousBoxes()
{
    // we have to go over all child nodes and remove anonymous boxes, that do _not_
    // have inline children to keep the tree flat
    RenderObject *child = firstChild();
    while( child ) {
	RenderObject *next = child->nextSibling();
	
	if ( child->isRenderBlock() && child->isAnonymousBlock() && !child->continuation() && !child->childrenInline() && !child->isTableCell() ) {
	    RenderObject *firstAnChild = child->firstChild();
	    RenderObject *lastAnChild = child->lastChild();
	    if ( firstAnChild ) {
		RenderObject *o = firstAnChild;
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
	    if ( child == firstChild() )
		m_first = firstAnChild;
	    if ( child == lastChild() )
		m_last = lastAnChild;
	    child->setParent( 0 );
	    child->setPreviousSibling( 0 );
	    child->setNextSibling( 0 );
	    if ( !child->isText() ) {
		RenderContainer *c = static_cast<RenderContainer *>(child);
		c->m_first = 0;
		c->m_next = 0;
	    }
	    child->destroy();
	}
	child = next;
    }
    if ( parent() )
	parent()->removeLeftoverAnonymousBoxes();
}

VisiblePosition RenderContainer::positionForCoordinates(int _x, int _y)
{
    // no children...return this render object's element, if there is one, and offset 0
    if (!firstChild())
        return VisiblePosition(element(), 0, DOWNSTREAM);

    // look for the geometrically-closest child and pass off to that child
    int min = INT_MAX;
    RenderObject *closestRenderer = 0;
    for (RenderObject *renderer = firstChild(); renderer; renderer = renderer->nextSibling()) {
        if (!renderer->firstChild() && !renderer->isInline() && !renderer->isBlockFlow())
            continue;

        int absx, absy;
        renderer->absolutePosition(absx, absy);
        
        int top = absy + borderTop() + paddingTop();
        int bottom = top + renderer->contentHeight();
        int left = absx + borderLeft() + paddingLeft();
        int right = left + renderer->contentWidth();
        
        int cmp;
        cmp = abs(_y - top);    if (cmp < min) { closestRenderer = renderer; min = cmp; }
        cmp = abs(_y - bottom); if (cmp < min) { closestRenderer = renderer; min = cmp; }
        cmp = abs(_x - left);   if (cmp < min) { closestRenderer = renderer; min = cmp; }
        cmp = abs(_x - right);  if (cmp < min) { closestRenderer = renderer; min = cmp; }
    }
    
    if (closestRenderer)
        return closestRenderer->positionForCoordinates(_x, _y);
    
    return VisiblePosition(element(), 0, DOWNSTREAM);
}

QValueList<IntRect> RenderContainer::lineBoxRects()
{
    if (!firstChild() && (isInline() || isAnonymousBlock())) {
        QValueList<IntRect> rects;
        int x = 0, y = 0;
        absolutePosition(x, y);
        absoluteRects(rects, x, y);
        return rects;
    }

    if (!firstChild())
        return QValueList<IntRect>();

    QValueList<IntRect> rects;
    for (RenderObject *child = firstChild(); child; child = child->nextSibling()) {
        if (child->isText() || child->isInline() || child->isAnonymousBlock()) {
            int x = 0, y = 0;
            child->absolutePosition(x, y);
            child->absoluteRects(rects, x, y);
        }
    }

    return rects;
}

#undef DEBUG_LAYOUT

}
