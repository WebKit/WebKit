/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2002 Apple Computer, Inc.
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

//#define DEBUG_LAYOUT

#include "render_container.h"
#include "render_table.h"
#include "render_text.h"
#include "render_image.h"
#include "render_root.h"

#include <kdebug.h>
#include <assert.h>

using namespace khtml;

RenderContainer::RenderContainer(DOM::NodeImpl* node)
    : RenderObject(node)
{
    m_first = 0;
    m_last = 0;
}


RenderContainer::~RenderContainer()
{
}

void RenderContainer::detach(RenderArena* renderArena)
{
    if (continuation())
        continuation()->detach(renderArena);
    
    RenderObject* next;
    for(RenderObject* n = m_first; n; n = next ) {
        n->removeFromSpecialObjects();
        n->setParent(0);
        next = n->nextSibling();
        n->detach(renderArena);
    }
    m_first = 0;
    m_last = 0;

    RenderObject::detach(renderArena);
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
        case LIST_ITEM:
        case RUN_IN:
        case COMPACT:
        case MARKER:
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
#if APPLE_CHANGES
            // I'm not 100% sure this is the best way to fix this, but without this
            // change we recurse infinitely when trying to render the CSS2 test page:
            // http://www.bath.ac.uk/%7Epy8ieh/internet/eviltests/htmlbodyheadrendering2.html.
            // See Radar 2925291.
            if ( isTableCell() && !firstChild() && !newChild->isTableCell() )
                needsTable = false;
#endif
            break;
        case NONE:
            kdDebug( 6000 ) << "error in RenderObject::addChild()!!!!" << endl;
            break;
        }
    }

    if ( needsTable ) {
        RenderTable *table;
        if( !beforeChild )
            beforeChild = lastChild();
        if( beforeChild && beforeChild->isAnonymousBox() && beforeChild->isTable() )
            table = static_cast<RenderTable *>(beforeChild);
        else {
            //kdDebug( 6040 ) << "creating anonymous table" << endl;
            table = new (renderArena()) RenderTable(0 /* is anonymous */);
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(style());
            newStyle->setDisplay(TABLE);
            table->setStyle(newStyle);
            table->setIsAnonymousBox(true);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else {
	// just add it...
	insertChildNode(newChild, beforeChild);
    }
    newChild->setLayouted( false );
    newChild->setMinMaxKnown( false );
}

RenderObject* RenderContainer::removeChildNode(RenderObject* oldChild)
{
    KHTMLAssert(oldChild->parent() == this);

    // Keep our layer hierarchy updated.
    oldChild->removeLayers(enclosingLayer());
   
    // if oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers

    // ### This is not the "proper" solution... ideally the selection should be maintained
    // based on DOM Nodes and a Range, which gets adjusted appropriately when nodes are
    // deleted/inserted near etc. But this at least prevents crashes caused when the start
    // or end of the selection is deleted and then accessed when the user next selects
    // something.

    if (oldChild->isSelectionBorder()) {
        RenderObject *root = oldChild;
        while (root && root->parent())
            root = root->parent();
        if (root->isRoot()) {
            static_cast<RenderRoot*>(root)->clearSelection();
        }
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

    setMinMaxKnown( false );
    setLayouted( false );
    
    return oldChild;
}

void RenderContainer::removeChild(RenderObject *oldChild)
{
    removeChildNode(oldChild);
    setLayouted(false);
}


void RenderContainer::insertPseudoChild(RenderStyle::PseudoId type, RenderObject* child, RenderObject* beforeChild)
{

    if (child->isText())
        return;

    RenderStyle* pseudo = child->style()->getPseudoStyle(type);

    if (pseudo)
    {
        if (pseudo->contentType()==CONTENT_TEXT)
        {
            RenderObject* po = new (renderArena()) RenderFlow(0 /* anonymous box */);
            po->setParent(this); // Set the parent now, so setStyle will be able to find a renderArena.
            po->setStyle(pseudo);
            po->setParent(0); // Unset the parent to avoid asserting in addChild.
            addChild(po, beforeChild);
            
            RenderText* t = new (renderArena()) RenderText(0 /*anonymous object */, pseudo->contentText());
            t->setParent(po); // Set the parent now, so setStyle will be able to find a renderArena.
            t->setStyle(pseudo);
            t->setParent(0); // Unset the parent to avoid asserting in addChild.
            po->addChild(t);
            
//            kdDebug() << DOM::DOMString(pseudo->contentText()).string() << endl;

            t->close();
            po->close();
        }
        else if (pseudo->contentType()==CONTENT_OBJECT)
        {
            RenderObject* po = new (renderArena()) RenderImage(0);
            po->setParent(this); // Set the parent now, so setStyle will be able to find a renderArena.
            po->setStyle(pseudo);
            po->setParent(0); // Unset the parent to avoid asserting in addChild.
            addChild(po, beforeChild);
            po->close();
        }

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
    
    // Keep our layer hierarchy updated.
    newChild->appendLayers(enclosingLayer());
         
    newChild->setMinMaxKnown( false );
    newChild->setLayouted( false );
}

void RenderContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild)
{
    if(!beforeChild) {
        appendChildNode(child);
        return;
    }

    KHTMLAssert(!child->parent());
    while ( beforeChild->parent() != this && beforeChild->parent()->isAnonymousBox() )
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
    // XXX Need this to do an insertion and really find the right place to
    // put the new layer. Not a big deal though. -dwh
    child->appendLayers(enclosingLayer());
           
    child->setMinMaxKnown( false );
    child->setLayouted( false );
}


void RenderContainer::layout()
{
    KHTMLAssert( !layouted() );
    KHTMLAssert( minMaxKnown() );

    RenderObject *child = firstChild();
    while( child ) {
        if( !child->layouted() )
            child->layout();
        child = child->nextSibling();
    }
    setLayouted();
}

void RenderContainer::removeLeftoverAnonymousBoxes()
{
    // we have to go over all child nodes and remove anonymous boxes, that do _not_
    // have inline children to keep the tree flat
    RenderObject *child = firstChild();
    while( child ) {
	RenderObject *next = child->nextSibling();
	
	if ( child->isFlow() && child->isAnonymousBox() && !child->continuation() && !child->childrenInline() ) {
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
	    child->detach(renderArena());
	}
	child = next;
    }
    if ( parent() )
	parent()->removeLeftoverAnonymousBoxes();
}
    
#undef DEBUG_LAYOUT
