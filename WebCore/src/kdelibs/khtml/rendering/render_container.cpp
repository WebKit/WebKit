/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
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
 * $Id$
 */

#include "render_container.h"
#include "render_table.h"

#include <kdebug.h>

using namespace khtml;

RenderContainer::RenderContainer()
    : RenderObject()
{
    m_first = 0;
    m_last = 0;
}


RenderContainer::~RenderContainer()
{
    RenderObject* next;
    for(RenderObject* n = m_first; n; n = next ) {
	n->removeFromSpecialObjects();
        n->setParent(0);
        next = n->nextSibling();
        n->detach();
    }
    m_first = 0;
    m_last = 0;
}

void RenderContainer::addChild(RenderObject *newChild, RenderObject *beforeChild)
{
#ifdef DEBUG_LAYOUT
    kdDebug( 6040 ) << renderName() << "(RenderObject)::addChild( " << newChild->renderName() << ", "
                       (beforeChild ? beforeChild->renderName() : "0") << " )" << endl;
#endif

    if(parsing())
        newChild->setParsing();

    bool needsTable = false;

    if(!newChild->isText()) {
        switch(newChild->style()->display()) {
        case INLINE:
        case BLOCK:
        case LIST_ITEM:
        case RUN_IN:
        case COMPACT:
        case MARKER:
        case TABLE:
        case INLINE_TABLE:
        case KONQ_RULER:
            break;
        case TABLE_COLUMN_GROUP:
        case TABLE_COLUMN:
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
//          kdDebug( 6040 ) << "creating anonymous table" << endl;
            table = new RenderTable;
            RenderStyle *newStyle = new RenderStyle();
            newStyle->inheritFrom(m_style);
            newStyle->setDisplay(TABLE);
            table->setStyle(newStyle);
            table->setIsAnonymousBox(true);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
        return;
    }

    // just add it...
    insertChildNode(newChild, beforeChild);
}

RenderObject* RenderContainer::removeChildNode(RenderObject* oldChild)
{
    ASSERT(oldChild->parent() == this);
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

    return oldChild;
}

void RenderContainer::removeChild(RenderObject *oldChild)
{
    removeChildNode(oldChild);
    setLayouted(false);
    if(containsWidget()) {
        bool anotherone = false;
        for(RenderObject* o = firstChild(); o; o = o->nextSibling()) {
            if(o->isWidget() || o->containsWidget()) {
                anotherone = true;
                break;
            }
        }
        if(!anotherone) {
            setContainsWidget(false);
            // ### propagate to parent!!
        }
    }
}

void RenderContainer::appendChildNode(RenderObject* newChild)
{
    ASSERT(newChild->parent() == 0);
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

    if(newChild->isWidget())
    {
        RenderObject* o = this;
        while(o) {
            o->setContainsWidget();
            o = o->parent();
        }
    }
}

void RenderContainer::insertChildNode(RenderObject* child, RenderObject* beforeChild)
{
    if(!beforeChild) {
        appendChildNode(child);
	return;
    }

    ASSERT(!child->parent());
    ASSERT(beforeChild->parent() == this);

    if(beforeChild == firstChild())
        setFirstChild(child);

    RenderObject* prev = beforeChild->previousSibling();
    child->setNextSibling(beforeChild);
    beforeChild->setPreviousSibling(child);
    if(prev) prev->setNextSibling(child);
    child->setPreviousSibling(prev);

    child->setParent(this);

    if(child->isWidget())
    {
        RenderObject* o = this;
        while(o) {
            o->setContainsWidget();
            o = o->parent();
        }
    }
}


