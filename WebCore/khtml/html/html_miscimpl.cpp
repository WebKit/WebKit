/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
// -------------------------------------------------------------------------
#include "html/html_miscimpl.h"
#include "html/html_formimpl.h"

#include "misc/htmlhashes.h"
#include "dom/dom_node.h"

using namespace DOM;

#include <kdebug.h>

HTMLBaseFontElementImpl::HTMLBaseFontElementImpl(DocumentPtr *doc)
    : HTMLElementImpl(doc)
{
}

HTMLBaseFontElementImpl::~HTMLBaseFontElementImpl()
{
}

NodeImpl::Id HTMLBaseFontElementImpl::id() const
{
    return ID_BASEFONT;
}

// -------------------------------------------------------------------------

HTMLCollectionImpl::HTMLCollectionImpl(NodeImpl *_base, int _type)
{
    base = _base;
    base->ref();
    type = _type;
    currentItem = 0L;
    idsDone = false;
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
                    if(!e->getAttribute(ATTR_HREF).isNull())
                        len++;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name and all elements with an id attribute
                if(e->id() == ID_A) {
                    if(!e->getAttribute(ATTR_NAME).isNull())
                        len++;
                }
                break;
            case DOC_ALL:      // "all" elements
                len++;
                break;
            case NODE_CHILDREN: // first-level children
                len++;
                deep = false;
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
                    if(!e->getAttribute(ATTR_HREF).isNull())
                        len++;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name or an id attribute
                if(e->id() == ID_A)
                    if(!e->getAttribute(ATTR_NAME).isNull())
                        len++;
                break;
            case DOC_ALL:
                len++;
                break;
            case NODE_CHILDREN:
                len++;
                deep = false;
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

NodeImpl *HTMLCollectionImpl::firstItem() const
{
    int pos = 0;
    currentItem = getItem(base->firstChild(), 0, pos);
    return currentItem;
}

NodeImpl *HTMLCollectionImpl::nextItem() const
{
    int pos = 0;
    // Look for the 'second' item. The first one is currentItem, already given back.
    NodeImpl *retval = getItem(currentItem, 1, pos);
    if (retval)
    {
        currentItem = retval;
        return retval;
    }
    // retval was 0, means we have to go up
    while( !retval && currentItem && currentItem->parentNode()
           && currentItem->parentNode() != base )
    {
        currentItem = currentItem->parentNode();
        if (currentItem->nextSibling())
        {
            // ... and to take the first one from there
            pos = 0;
            retval = getItem(currentItem->nextSibling(), 0, pos);
        }
    }
    currentItem = retval;
    return currentItem;
}

NodeImpl *HTMLCollectionImpl::getNamedItem( NodeImpl *current, int attr_id,
                                            const DOMString &name, bool caseSensitive ) const
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
                    if(!e->getAttribute(ATTR_HREF).isNull())
                        check = true;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name
                if(e->id() == ID_A)
                    if(!e->getAttribute(ATTR_NAME).isNull())
                        check = true;
                break;
            case DOC_ALL:
                check = true;
                break;
            case NODE_CHILDREN:
                check = true;
                deep = false;
                break;
            default:
                kdDebug( 6030 ) << "Error in HTMLCollection, wrong tagId!" << endl;
                break;
            }
            if (check) {
                bool found;
                if (caseSensitive)
                    found = e->getAttribute(attr_id) == name;
                else
                    found = e->getAttribute(attr_id).lower() == name.lower();
                if (found) {
                    //kdDebug( 6030 ) << "found node: " << e << " " << current << " " << e->id() << " " << e->tagName().string() << endl;
                    return current;
                }
            }
            NodeImpl *retval = 0;
            if(deep && current->firstChild())
                retval = getNamedItem(current->firstChild(), attr_id, name, caseSensitive);
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

NodeImpl *HTMLCollectionImpl::namedItem( const DOMString &name, bool caseSensitive ) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    idsDone = false;
    currentItem = getNamedItem(base->firstChild(), ATTR_ID, name, caseSensitive);
    if(currentItem)
        return currentItem;
    idsDone = true;
    currentItem = getNamedItem(base->firstChild(), ATTR_NAME, name, caseSensitive);
    return currentItem;
}

NodeImpl *HTMLCollectionImpl::nextNamedItem( const DOMString &name ) const
{
    // nextNamedItemInternal can return an item that has both id=<name> and name=<name>
    // Here, we have to filter out such cases.
    NodeImpl *impl = nextNamedItemInternal( name );
    if (!idsDone) // looking for id=<name> -> no filtering
        return impl;
    // looking for name=<name> -> filter out if id=<name>
    bool ok = false;
    while (impl && !ok)
    {
        if(impl->nodeType() == Node::ELEMENT_NODE)
        {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(impl);
            ok = (e->getAttribute(ATTR_ID) != name);
            if (!ok)
                impl = nextNamedItemInternal( name );
        } else // can't happen
            ok = true;
    }
    return impl;
}

NodeImpl *HTMLCollectionImpl::nextNamedItemInternal( const DOMString &name ) const
{
    //kdDebug() << "\nHTMLCollectionImpl::nextNamedItem starting at " << currentItem << endl;
    // Go to next item first (to avoid returning the same)
    currentItem = nextItem();
    //kdDebug() << "*HTMLCollectionImpl::nextNamedItem next item is " << currentItem << endl;

    if ( currentItem )
    {
        // Then look for next matching named item
        NodeImpl *retval = getNamedItem(currentItem, idsDone ? ATTR_NAME : ATTR_ID, name);
        if ( retval )
        {
            //kdDebug() << "*HTMLCollectionImpl::nextNamedItem found " << retval << endl;
            currentItem = retval;
            return retval;
        }

        // retval was 0, means we have to go up
        while( !retval && currentItem->parentNode()
               && currentItem->parentNode() != base )
        {
            currentItem = currentItem->parentNode();
            if (currentItem->nextSibling())
            {
                // ... and to take the first one from there
                retval = getNamedItem(currentItem->nextSibling(), idsDone ? ATTR_NAME : ATTR_ID, name);
            }
        }
        if ( retval )
        {
            //kdDebug() << "*HTMLCollectionImpl::nextNamedItem found after going up " << retval << endl;
            currentItem = retval;
            return currentItem;
        }
    }

    if ( idsDone )
        return 0;
    // After doing all ATTR_ID, do ATTR_NAME
    //kdDebug() << "*HTMLCollectionImpl::nextNamedItem going to ATTR_NAME now" << endl;
    idsDone = true;
    currentItem = getNamedItem(base->firstChild(), ATTR_NAME, name);
    return currentItem;

}

// -----------------------------------------------------------------------------

unsigned long HTMLFormCollectionImpl::calcLength(NodeImpl*) const
{
    QPtrList<HTMLGenericFormElementImpl> l = static_cast<HTMLFormElementImpl*>( base )->formElements;

    int len = 0;
    for ( unsigned i = 0; i < l.count(); i++ )
        if ( l.at( i )->isEnumeratable() )
            ++len;

    return len;
}

NodeImpl* HTMLFormCollectionImpl::getItem(NodeImpl *, int index, int&) const
{
    QPtrList<HTMLGenericFormElementImpl> l = static_cast<HTMLFormElementImpl*>( base )->formElements;

    for ( unsigned i = 0; i < l.count(); i++ ) {

        if( l.at( i )->isEnumeratable() ) {
            if ( !index )
                return l.at( i );

            --index;
        }
    }

    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedItem(NodeImpl*, int attr_id, const DOMString& name, bool caseSensitive) const
{
    currentPos = 0;
    return getNamedFormItem( attr_id, name, 0, caseSensitive );
}

NodeImpl* HTMLFormCollectionImpl::getNamedFormItem(int attr_id, const DOMString& name, int duplicateNumber, bool caseSensitive) const
{
    if(base->nodeType() == Node::ELEMENT_NODE)
    {
        HTMLElementImpl* e = static_cast<HTMLElementImpl*>(base);
        if(e->id() == ID_FORM)
        {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(e);

            for(HTMLGenericFormElementImpl* e = f->formElements.first(); e; e = f->formElements.next())
                if(e->isEnumeratable()) {
                    bool found;
                    if (caseSensitive)
                        found = e->getAttribute(attr_id) == name;
                    else
                        found = e->getAttribute(attr_id).lower() == name.lower();
                    if (found) {
                        if (!duplicateNumber)
                            return e;
                        --duplicateNumber;
                    }
                }
        }
        NodeImpl* retval = getNamedImgItem( base->firstChild(), attr_id, name, duplicateNumber, caseSensitive );
        if ( retval )
            return retval;
    }
    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedImgItem(NodeImpl* current, int attr_id, const DOMString& name, int& duplicateNumber, bool caseSensitive) const
{
    // strange case. IE and NS allow to get hold of <img> tags,
    // but they don't include them in the elements() collection.
    while ( current )
    {
        if(current->nodeType() == Node::ELEMENT_NODE)
        {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            if(e->id() == ID_IMG) {
                bool found;
                if (caseSensitive)
                    found = e->getAttribute(attr_id) == name;
                else
                    found = e->getAttribute(attr_id).lower() == name.lower();
                if (found)
                {
                    if (!duplicateNumber)
                        return current;
                    --duplicateNumber;
                }
            }
            if(current->firstChild())
            {
                // The recursion here is the reason why this is a separate method
                NodeImpl *retval = getNamedImgItem(current->firstChild(), attr_id, name, duplicateNumber, caseSensitive);
                if(retval)
                {
                    //kdDebug( 6030 ) << "got a return value " << retval << endl;
                    return retval;
                }
            }
        }
        current = current->nextSibling();
    } // while
    return 0;
}

NodeImpl * HTMLFormCollectionImpl::firstItem() const
{
    currentPos = 0;
    int dummy = 0;
    return getItem(0 /*base->firstChild() unused*/, currentPos, dummy);
}

NodeImpl * HTMLFormCollectionImpl::nextItem() const
{
    // This implementation loses the whole benefit of firstItem/nextItem :(
    int dummy = 0;
    return getItem(0 /*base->firstChild() unused*/, ++currentPos, dummy);
}

NodeImpl * HTMLFormCollectionImpl::nextNamedItemInternal( const DOMString &name ) const
{
    NodeImpl *retval = getNamedFormItem( idsDone ? ATTR_NAME : ATTR_ID, name, ++currentPos, true );
    if ( retval )
        return retval;
    if ( idsDone ) // we're done
        return 0;
    // After doing all ATTR_ID, do ATTR_NAME
    idsDone = true;
    return getNamedItem(base->firstChild(), ATTR_NAME, name, true);
}
