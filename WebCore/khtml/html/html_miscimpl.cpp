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
#include "html/html_imageimpl.h"
#include "html/html_documentimpl.h"

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
    idsDone = false;
    info = base->isDocumentNode() ? static_cast<HTMLDocumentImpl*>(base->getDocument())->collectionInfo(type) : new CollectionInfo;
}

HTMLCollectionImpl::~HTMLCollectionImpl()
{
    if (!base->isDocumentNode())
        delete info;
    base->deref();
}

void HTMLCollectionImpl::resetCollectionInfo() const
{
    unsigned int docversion = static_cast<HTMLDocumentImpl*>(base->getDocument())->domTreeVersion();
    if (info->version != docversion) {
        info->current = 0;
        info->position = 0;
        info->length = 0;
        info->haslength = false;
        info->version = docversion;
    }
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
                if(e->id() == ID_TD || e->id() == ID_TH)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR)
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
            case DOC_EMBEDS:   // all EMBED elements
                if(e->id() == ID_EMBED)
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
    resetCollectionInfo();
    if (!info->haslength) {
        info->length = calcLength(base->firstChild());
        info->haslength = true;
    }
    return info->length;
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
                if(e->id() == ID_TD || e->id() == ID_TH)
                    len++;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR)
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
            case DOC_EMBEDS:   // all EMBED elements
                if(e->id() == ID_EMBED)
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
     resetCollectionInfo();
     if (info->current && info->position == index) {
         return info->current;
     }
     if (info->haslength && info->length <= index) {
         return 0;
     }
     if (!info->current || info->position > index) {
         info->current = base->firstChild();
         info->position = 0;
         if (!info->current)
             return 0;
     }
     int pos = (int) info->position;
     NodeImpl *node = getItem(info->current, index, pos);
     while (!node && info->current->parentNode() && info->current->parentNode() != base) {
         info->current = info->current->parentNode();
         if (info->current->nextSibling())
             node = getItem(info->current->nextSibling(), index, pos);
         
     }
     info->current = node;
     info->position = index;
     return info->current;
}

NodeImpl *HTMLCollectionImpl::firstItem() const
{
     return item(0);
}

NodeImpl *HTMLCollectionImpl::nextItem() const
{
     resetCollectionInfo();
     int pos = 0;
 
     info->position = ~0;  // no position
     // Look for the 'second' item. The first one is currentItem, already given back.
     NodeImpl *retval = getItem(info->current, 1, pos);
     if (retval)
     {
         info->current = retval;
         return retval;
     }
     // retval was 0, means we have to go up
     while( !retval && info->current->parentNode()
            && info->current->parentNode() != base )
     {
         info->current = info->current->parentNode();
         if (info->current->nextSibling())
         {
             // ... and to take the first one from there
             pos = 0;
             retval = getItem(info->current->nextSibling(), 0, pos);
         }
      }
     info->current = retval;
     return info->current;
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
                if(e->id() == ID_TD || e->id() == ID_TH)
                    check = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR)
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
            case DOC_EMBEDS:   // all EMBED elements
                if(e->id() == ID_EMBED)
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
                    found = e->getAttribute(attr_id).domString().lower() == name.lower();
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
     resetCollectionInfo();
     info->position = ~0;  // no position
    idsDone = false;
    info->current = getNamedItem(base->firstChild(), ATTR_ID, name);
    if(info->current)
        return info->current;
    idsDone = true;
    info->current = getNamedItem(base->firstChild(), ATTR_NAME, name);
    return info->current;
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
    resetCollectionInfo();
    //kdDebug() << "\nHTMLCollectionImpl::nextNamedItem starting at " << info->current << endl;
    // Go to next item first (to avoid returning the same)
    nextItem(); // sets info->current and invalidates info->postion
    //kdDebug() << "*HTMLCollectionImpl::nextNamedItem next item is " << info->current << endl;

    if ( info->current )
    {
        // Then look for next matching named item
        NodeImpl *retval = getNamedItem(info->current, idsDone ? ATTR_NAME : ATTR_ID, name);
        if ( retval )
        {
            //kdDebug() << "*HTMLCollectionImpl::nextNamedItem found " << retval << endl;
            info->current = retval;
            return retval;
        }

        // retval was 0, means we have to go up
        while( !retval && info->current->parentNode()
               && info->current->parentNode() != base )
        {
            info->current = info->current->parentNode();
            if (info->current->nextSibling())
            {
                // ... and to take the first one from there
                retval = getNamedItem(info->current->nextSibling(), idsDone ? ATTR_NAME : ATTR_ID, name);
            }
        }
        if ( retval )
        {
            //kdDebug() << "*HTMLCollectionImpl::nextNamedItem found after going up " << retval << endl;
            info->current = retval;
            return info->current;
        }
    }

    if ( idsDone )
        return 0;
    // After doing all ATTR_ID, do ATTR_NAME
    //kdDebug() << "*HTMLCollectionImpl::nextNamedItem going to ATTR_NAME now" << endl;
    idsDone = true;
    info->current = getNamedItem(base->firstChild(), ATTR_NAME, name);
    return info->current;

}

// -----------------------------------------------------------------------------

HTMLFormCollectionImpl::FormCollectionInfo::FormCollectionInfo()
{
    reset();
}

void::HTMLFormCollectionImpl::FormCollectionInfo::reset()
{
    elementsArrayPosition = 0;
}

void HTMLFormCollectionImpl::resetCollectionInfo() const
{
    unsigned int docversion = static_cast<HTMLDocumentImpl*>(base->getDocument())->domTreeVersion();
    if (info->version != docversion) {
        formInfo.reset();
    }
    HTMLCollectionImpl::resetCollectionInfo();
}

unsigned long HTMLFormCollectionImpl::calcLength(NodeImpl*) const
{
    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>( base )->formElements;

    int len = 0;
    for ( unsigned i = 0; i < l.count(); i++ )
        if ( l.at( i )->isEnumeratable() )
            ++len;

    return len;
}

NodeImpl *HTMLFormCollectionImpl::item(unsigned long index) const
{
    resetCollectionInfo();

    if (info->current && info->position == index) {
        return info->current;
    }
    if (info->haslength && info->length <= index) {
        return 0;
    }
    if (!info->current || info->position > index) {
        info->current = 0;
        info->position = 0;
        formInfo.elementsArrayPosition = 0;
    }

    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>( base )->formElements;
    unsigned currentIndex = info->position;
    
    for (unsigned i = formInfo.elementsArrayPosition; i < l.count(); i++) {
        if (l[i]->isEnumeratable() ) {
            if (index == currentIndex) {
                info->position = index;
                info->current = l[i];
                formInfo.elementsArrayPosition = i;
                return l[i];
            }

            currentIndex++;
        }
    }

    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedItem(NodeImpl*, int attr_id, const DOMString& name, bool caseSensitive) const
{
    info->position = 0;
    return getNamedFormItem( attr_id, name, 0, caseSensitive );
}

NodeImpl* HTMLFormCollectionImpl::getNamedFormItem(int attr_id, const DOMString& name, int duplicateNumber, bool caseSensitive) const
{
    if(base->nodeType() == Node::ELEMENT_NODE)
    {
        HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(base);
        bool foundInputElements = false;
        if(baseElement->id() == ID_FORM)
        {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
            for (unsigned i = 0; i < f->formElements.count(); ++i) {
                HTMLGenericFormElementImpl* e = f->formElements[i];
                if (e->isEnumeratable()) {
                    bool found;
                    if (caseSensitive)
                        found = e->getAttribute(attr_id) == name;
                    else
                        found = e->getAttribute(attr_id).domString().lower() == name.lower();
                    if (found) {
                        foundInputElements = true;
                        if (!duplicateNumber)
                            return e;
                        --duplicateNumber;
                    }
                }
            }
        }

        if ( !foundInputElements )
        {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);

            for(unsigned i = 0; i < f->imgElements.count(); ++i)
            {
                HTMLImageElementImpl* e = f->imgElements[i];
                bool found;
                if (caseSensitive)
                    found = e->getAttribute(attr_id) == name;
                else
                    found = e->getAttribute(attr_id).domString().lower() == name.lower();
                if (found) {
                    if (!duplicateNumber)
                        return e;
                    --duplicateNumber;
                }
            }
        }
    }
    return 0;
}

NodeImpl * HTMLFormCollectionImpl::firstItem() const
{
    return item(0);
}

NodeImpl * HTMLFormCollectionImpl::nextItem() const
{
    return item(info->position + 1);
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
