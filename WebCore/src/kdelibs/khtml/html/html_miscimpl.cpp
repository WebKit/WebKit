/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
// -------------------------------------------------------------------------
#include "html_miscimpl.h"
#include "html_formimpl.h"

#include "htmlhashes.h"
#include "dom_node.h"
using namespace DOM;

#include <kdebug.h>

HTMLBaseFontElementImpl::HTMLBaseFontElementImpl(DocumentPtr *doc) : HTMLElementImpl(doc)
{
}

HTMLBaseFontElementImpl::~HTMLBaseFontElementImpl()
{
}

const DOMString HTMLBaseFontElementImpl::nodeName() const
{
    return "BASEFONT";
}

ushort HTMLBaseFontElementImpl::id() const
{
    return ID_BASEFONT;
}

// -------------------------------------------------------------------------

HTMLCollectionImpl::HTMLCollectionImpl(NodeImpl *_base, int _type)
{
    base = _base;
    base->ref();
    type = _type;
}

HTMLCollectionImpl::~HTMLCollectionImpl()
{
    base->deref();
}

unsigned long HTMLCollectionImpl::calcLength(NodeImpl *current) const
{
    unsigned long len = 0;
    while(current)
    {
        if(current->nodeType() == Node::ELEMENT_NODE)
        {
            bool deep = true;
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            switch(type)
            {
            case DOC_IMAGES:
                if(e->id() == ID_IMG)
                    len++;
                break;
            case DOC_FORMS:
                if(e->id() == ID_FORM)
                    len++;
                break;
            case TABLE_TBODIES:
                if(e->id() == ID_TBODY)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TR_CELLS:
                if(e->id() == ID_TD)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR || e->id() == ID_TH)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case SELECT_OPTIONS:
                if(e->id() == ID_OPTION)
                    len++;
                break;
            case MAP_AREAS:
                if(e->id() == ID_AREA)
                    len++;
                break;
            case DOC_APPLETS:   // all OBJECT and APPLET elements
                if(e->id() == ID_OBJECT || e->id() == ID_APPLET)
                    len++;
                break;
            case DOC_LINKS:     // all A _and_ AREA elements with a value for href
                if(e->id() == ID_A || e->id() == ID_AREA)
                    if(e->getAttribute(ATTR_HREF) != 0)
                        len++;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name and all elements with an id attribute
                if ( e->hasID() )
                    len++;
                else if(e->id() == ID_A) {
                    if(e->getAttribute(ATTR_NAME) != 0)
                        len++;
                }
                break;
            case DOC_ALL:      // "all" elements
                len++;
                break;
            default:
                kdDebug( 6030 ) << "Error in HTMLCollection, wrong tagId!" << endl;
            }
            if(deep && current->firstChild())
                len += calcLength(current->firstChild());
        }
        current = current->nextSibling();
    }
    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time...
unsigned long HTMLCollectionImpl::length() const
{
    return calcLength(base->firstChild());
}

NodeImpl *HTMLCollectionImpl::getItem(NodeImpl *current, int index, int &len) const
{
    while(current)
    {
        if(current->nodeType() == Node::ELEMENT_NODE)
        {
            bool deep = true;
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            switch(type)
            {
            case DOC_IMAGES:
                if(e->id() == ID_IMG)
                    len++;
                break;
            case DOC_FORMS:
                if(e->id() == ID_FORM)
                    len++;
                break;
            case TABLE_TBODIES:
                if(e->id() == ID_TBODY)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TR_CELLS:
                if(e->id() == ID_TD)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR || e->id() == ID_TH)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case SELECT_OPTIONS:
                if(e->id() == ID_OPTION)
                    len++;
                break;
            case MAP_AREAS:
                if(e->id() == ID_AREA)
                    len++;
                break;
            case DOC_APPLETS:   // all OBJECT and APPLET elements
                if(e->id() == ID_OBJECT || e->id() == ID_APPLET)
                    len++;
                break;
            case DOC_LINKS:     // all A _and_ AREA elements with a value for href
                if(e->id() == ID_A || e->id() == ID_AREA)
                    if(e->getAttribute(ATTR_HREF) != 0)
                        len++;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name or an id attribute
                if( e->hasID() )
                    len++;
                else if(e->id() == ID_A)
                    if(e->getAttribute(ATTR_NAME) != 0)
                        len++;
                break;
            case DOC_ALL:
                len++;
                break;
            default:
                kdDebug( 6030 ) << "Error in HTMLCollection, wrong tagId!" << endl;
            }
            if(len == (index + 1)) return current;
            NodeImpl *retval=0;
            if(deep && current->firstChild())
                retval = getItem(current->firstChild(), index, len);
            if(retval) return retval;
        }
        current = current->nextSibling();
    }
    return 0;
}

NodeImpl *HTMLCollectionImpl::item( unsigned long index ) const
{
    int pos = 0;
    return getItem(base->firstChild(), index, pos);
}

NodeImpl *HTMLCollectionImpl::getNamedItem( NodeImpl *current, int attr_id,
                                            const DOMString &name ) const
{
    if(name.isEmpty())
        return 0;

    while(current)
    {
        if(current->nodeType() == Node::ELEMENT_NODE)
        {
            bool deep = true;
            bool check = false;
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            switch(type)
            {
            case DOC_IMAGES:
                if(e->id() == ID_IMG)
                    check = true;
                break;
            case DOC_FORMS:
                if(e->id() == ID_FORM)
                    check = true;
                break;
            case TABLE_TBODIES:
                if(e->id() == ID_TBODY)
                    check = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TR_CELLS:
                if(e->id() == ID_TD)
                    check = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR || e->id() == ID_TH)
                    check = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case SELECT_OPTIONS:
                if(e->id() == ID_OPTION)
                    check = true;
                break;
            case MAP_AREAS:
                if(e->id() == ID_AREA)
                    check = true;
                break;
            case DOC_APPLETS:   // all OBJECT and APPLET elements
                if(e->id() == ID_OBJECT || e->id() == ID_APPLET)
                    check = true;
                break;
            case DOC_LINKS:     // all A _and_ AREA elements with a value for href
                if(e->id() == ID_A || e->id() == ID_AREA)
                    if(e->getAttribute(ATTR_HREF) != 0)
                        check = true;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name
                if( e->hasID() )
                    check = true;
                else if(e->id() == ID_A)
                    if(e->getAttribute(ATTR_NAME) != 0)
                        check = true;
                break;
            case DOC_ALL:
                check = true;
                break;
            default:
                kdDebug( 6030 ) << "Error in HTMLCollection, wrong tagId!" << endl;
                break;
            }
            if(check && e->getAttribute(attr_id) == name)
            {
                //kdDebug( 6030 ) << "found node: " << e << " " << current << " " << e->id() << endl;
                return current;
            }
            NodeImpl *retval = 0;
            if(deep && current->firstChild())
                retval = getNamedItem(current->firstChild(), attr_id, name);
            if(retval)
            {
                //kdDebug( 6030 ) << "got a return value " << retval << endl;
                return retval;
            }
        }
        current = current->nextSibling();
    }
    return 0;
}

NodeImpl *HTMLCollectionImpl::namedItem( const DOMString &name ) const
{
    NodeImpl *n;
    n = getNamedItem(base->firstChild(), ATTR_ID, name);
    if(n) return n;
    return getNamedItem(base->firstChild(), ATTR_NAME, name);
}


// -----------------------------------------------------------------------------

unsigned long HTMLFormCollectionImpl::calcLength(NodeImpl*) const
{
    QList<HTMLGenericFormElementImpl> l = static_cast<HTMLFormElementImpl*>( base )->formElements;

    int len = 0;
    for ( unsigned i = 0; i < l.count(); i++ )
        if ( l.at( i )->isEnumeratable() )
            ++len;

    return len;
}

NodeImpl* HTMLFormCollectionImpl::getItem(NodeImpl *, int index, int&) const
{
    QList<HTMLGenericFormElementImpl> l = static_cast<HTMLFormElementImpl*>( base )->formElements;

    for ( unsigned i = 0; i < l.count(); i++ ) {

        if( l.at( i )->isEnumeratable() ) {
            if ( !index )
                return l.at( i );

            --index;
        }
    }

    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedItem(NodeImpl*, int attr_id, const DOMString& name) const
{
    if(base->nodeType() == Node::ELEMENT_NODE)
    {
        HTMLElementImpl* e = static_cast<HTMLElementImpl*>(base);
        if(e->id() == ID_FORM)
        {
            HTMLGenericFormElementImpl* element;
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(e);

            for(element = f->formElements.first(); element; element = f->formElements.next())
                if(element->isEnumeratable() && element->getAttribute(attr_id) == name)
                    return element;
        }
    }

    return 0;
}
