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


NodeImpl *HTMLCollectionImpl::traverseNextItem(NodeImpl *current) const
{
    current = current->traverseNextNode();

    while (current) {
        if(current->nodeType() == Node::ELEMENT_NODE) {
            bool found = false;
            bool deep = true;
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            switch(type) {
            case DOC_IMAGES:
                if(e->id() == ID_IMG)
                    found = true;
                break;
            case DOC_FORMS:
                if(e->id() == ID_FORM)
                    found = true;
                break;
            case TABLE_TBODIES:
                if(e->id() == ID_TBODY)
                    found = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TR_CELLS:
                if(e->id() == ID_TD || e->id() == ID_TH)
                    found = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if(e->id() == ID_TR)
                    found = true;
                else if(e->id() == ID_TABLE)
                    deep = false;
                break;
            case SELECT_OPTIONS:
                if(e->id() == ID_OPTION)
                    found = true;
                break;
            case MAP_AREAS:
                if(e->id() == ID_AREA)
                    found = true;
                break;
            case DOC_APPLETS:   // all OBJECT and APPLET elements
                if(e->id() == ID_OBJECT || e->id() == ID_APPLET)
                    found = true;
                break;
            case DOC_EMBEDS:   // all EMBED elements
                if(e->id() == ID_EMBED)
                    found = true;
                break;
            case DOC_LINKS:     // all A _and_ AREA elements with a value for href
                if(e->id() == ID_A || e->id() == ID_AREA)
                    if(!e->getAttribute(ATTR_HREF).isNull())
                        found = true;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name or an id attribute
                if(e->id() == ID_A)
                    if(!e->getAttribute(ATTR_NAME).isNull())
                        found = true;
                break;
            case DOC_ALL:
                found = true;
                break;
            case NODE_CHILDREN:
                found = true;
                deep = false;
                break;
            default:
                kdDebug( 6030 ) << "Error in HTMLCollection, wrong tagId!" << endl;
            }
            if (found) {
                return current;
            }
            if (deep) {
                current = current->traverseNextNode(base);
                continue;
            } 
        }
        current = current->traverseNextSibling(base);
    }
    return 0;
}


unsigned long HTMLCollectionImpl::calcLength() const
{
    unsigned long len = 0;

    for (NodeImpl *current = traverseNextItem(base); current; current = traverseNextItem(current)) {
        len++;
    }

    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed
unsigned long HTMLCollectionImpl::length() const
{
    resetCollectionInfo();
    if (!info->haslength) {
        info->length = calcLength();
        info->haslength = true;
    }
    return info->length;
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
         info->current = traverseNextItem(base);
         info->position = 0;
         if (!info->current)
             return 0;
     }
     NodeImpl *node = info->current;
     for (unsigned pos = info->position; pos < index; pos++) {
         node = traverseNextItem(node);
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
 
     // Look for the 'second' item. The first one is currentItem, already given back.
     NodeImpl *retval = traverseNextItem(info->current);
     info->current = retval;
     info->position++;
     return retval;
}

bool HTMLCollectionImpl::checkForNameMatch(NodeImpl *node, bool checkName, const DOMString &name, bool caseSensitive) const
{
    ElementImpl *e = static_cast<ElementImpl *>(node);
    if (caseSensitive) {
        if (checkName) {
            return e->getAttribute(ATTR_NAME) == name && e->getAttribute(ATTR_ID) != name;
        } else {
            return e->getAttribute(ATTR_ID) == name;
        }
    } else {
        if (checkName) {
            return e->getAttribute(ATTR_NAME).domString().lower() == name.lower() &&
                e->getAttribute(ATTR_ID).domString().lower() != name.lower();
        } else {
            return e->getAttribute(ATTR_ID).domString().lower() == name.lower();
        }
    }
}


NodeImpl *HTMLCollectionImpl::namedItem( const DOMString &name, bool caseSensitive ) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    idsDone = false;

    NodeImpl *n;
    for (n = traverseNextItem(base); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, caseSensitive)) {
            break;
        }
    }
        
    info->current = n;
    if(info->current)
        return info->current;
    idsDone = true;

    for (n = traverseNextItem(base); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, caseSensitive)) {
            break;
        }
    }

    info->current = n;
    return info->current;
}

QValueList<Node> HTMLCollectionImpl::namedItems(const DOMString &name) const
{
    resetCollectionInfo();
    
    QValueList<Node> idResults;
    QValueList<Node> nameResults;

    for (NodeImpl *n = traverseNextItem(base); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, false, name, true)) {
            idResults.append(Node(n));
        } 
        if (checkForNameMatch(n, true, name, true)) {
            nameResults.append(Node(n));
        }
    }

    if (idResults.isEmpty())
        return nameResults;

    for (QValueListIterator<Node> iter = nameResults.begin(); iter != nameResults.end(); ++iter) {
        idResults.append(*iter);
    }

    return idResults;
}


NodeImpl *HTMLCollectionImpl::nextNamedItem( const DOMString &name ) const
{
    resetCollectionInfo();

    for (NodeImpl *n = traverseNextItem(info->current ? info->current : base); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, true)) {
            info->current = n;
            return n;
        }
    }
    
    if (idsDone) {
        info->current = 0; 
        return 0;
    }
    idsDone = true;

    for (NodeImpl *n = traverseNextItem(info->current ? info->current : base); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, true)) {
            info->current = n;
            return n;
        }
    }

    return 0;
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

unsigned long HTMLFormCollectionImpl::calcLength() const
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
    NodeImpl *retval = getNamedFormItem( idsDone ? ATTR_NAME : ATTR_ID, name, ++info->position, true );
    if ( retval )
        return retval;
    if ( idsDone ) // we're done
        return 0;
    // After doing all ATTR_ID, do ATTR_NAME
    idsDone = true;
    return getNamedItem(base->firstChild(), ATTR_NAME, name, true);
}

NodeImpl *HTMLFormCollectionImpl::namedItem( const DOMString &name, bool caseSensitive ) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    idsDone = false;
    info->current = getNamedItem(base->firstChild(), ATTR_ID, name, true);
    if(info->current)
        return info->current;
    idsDone = true;
    info->current = getNamedItem(base->firstChild(), ATTR_NAME, name, true);
    return info->current;
}


NodeImpl *HTMLFormCollectionImpl::nextNamedItem( const DOMString &name ) const
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

QValueList<Node> HTMLFormCollectionImpl::namedItems(const DOMString &name) const
{
    if (name.length()) {
        return QValueList<Node>();
    }

    resetCollectionInfo();

    QValueList<Node> idResults;
    QValueList<Node> nameResults;

    if (base->nodeType() != Node::ELEMENT_NODE ||static_cast<HTMLElementImpl*>(base)->id() != ID_FORM)
        return idResults;

    HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(base);

    bool foundInputElements = false;
    HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
    for (unsigned i = 0; i < f->formElements.count(); ++i) {
        HTMLGenericFormElementImpl* e = f->formElements[i];
        if (e->isEnumeratable()) {
            if (checkForNameMatch(e, false, name, true)) {
                idResults.append(Node(e));
                foundInputElements = true;
            } 
            if (checkForNameMatch(e, true, name, true)) {
                nameResults.append(Node(e));
                foundInputElements = true;
            }
        }
    }

    if (!foundInputElements) {
        HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);

        for (unsigned i = 0; i < f->imgElements.count(); ++i) {
            HTMLImageElementImpl* e = f->imgElements[i];

            if (checkForNameMatch(e, false, name, true)) {
                idResults.append(Node(e));
            } 
            if (checkForNameMatch(e, true, name, true)) {
                nameResults.append(Node(e));
            }
        }
    }

    if (idResults.isEmpty())
        return nameResults;

    for (QValueListIterator<Node> iter = nameResults.begin(); iter != nameResults.end(); ++iter) {
        idResults.append(*iter);
    }

    return idResults;
}

