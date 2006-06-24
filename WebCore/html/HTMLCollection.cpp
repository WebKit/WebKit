/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
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
#include "HTMLCollection.h"

#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLCollection::HTMLCollection(Node *_base, HTMLCollection::Type _type)
    : m_base(_base),
      type(_type),
      info(0),
      idsDone(false),
      m_ownsInfo(false)
{
    if (_base->isDocumentNode())
        info = _base->document()->collectionInfo(type);
}

HTMLCollection::~HTMLCollection()
{
    if (m_ownsInfo)
        delete info;
}

HTMLCollection::CollectionInfo::CollectionInfo() :
    version(0)
{
    reset();
}

HTMLCollection::CollectionInfo::~CollectionInfo()
{
    deleteAllValues(idCache);
    deleteAllValues(nameCache);
}

void HTMLCollection::CollectionInfo::reset()
{
    current = 0;
    position = 0;
    length = 0;
    haslength = false;
    elementsArrayPosition = 0;
    deleteAllValues(idCache);
    idCache.clear();
    deleteAllValues(nameCache);
    nameCache.clear();
    hasNameCache = false;
}

void HTMLCollection::resetCollectionInfo() const
{
    unsigned int docversion = static_cast<HTMLDocument*>(m_base->document())->domTreeVersion();

    if (!info) {
        info = new CollectionInfo;
        m_ownsInfo = true;
        info->version = docversion;
        return;
    }

    if (info->version != docversion) {
        info->reset();
        info->version = docversion;
    }
}


Node *HTMLCollection::traverseNextItem(Node *current) const
{
    assert(current);

    if (type == NodeChildren && m_base.get() != current)
        current = current->nextSibling();
    else
        current = current->traverseNextNode(m_base.get());

    while (current) {
        if (current->isElementNode()) {
            bool found = false;
            bool deep = true;
            HTMLElement *e = static_cast<HTMLElement *>(current);
            switch(type) {
            case DocImages:
                if (e->hasLocalName(imgTag))
                    found = true;
                break;
            case DocScripts:
                if (e->hasLocalName(scriptTag))
                    found = true;
                break;
            case DocForms:
                if(e->hasLocalName(formTag))
                    found = true;
                break;
            case TableTBodies:
                if (e->hasLocalName(tbodyTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case TRCells:
                if (e->hasLocalName(tdTag) || e->hasLocalName(thTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case TableRows:
            case TSectionRows:
                if (e->hasLocalName(trTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case SelectOptions:
                if (e->hasLocalName(optionTag))
                    found = true;
                break;
            case MapAreas:
                if (e->hasLocalName(areaTag))
                    found = true;
                break;
            case DocApplets:   // all OBJECT and APPLET elements
                if (e->hasLocalName(objectTag) || e->hasLocalName(appletTag))
                    found = true;
                break;
            case DocEmbeds:   // all EMBED elements
                if (e->hasLocalName(embedTag))
                    found = true;
                break;
            case DocObjects:   // all OBJECT elements
                if (e->hasLocalName(objectTag))
                    found = true;
                break;
            case DocLinks:     // all A _and_ AREA elements with a value for href
                if (e->hasLocalName(aTag) || e->hasLocalName(areaTag))
                    if (!e->getAttribute(hrefAttr).isNull())
                        found = true;
                break;
            case DocAnchors:      // all A elements with a value for name or an id attribute
                if (e->hasLocalName(aTag))
                    if (!e->getAttribute(nameAttr).isNull())
                        found = true;
                break;
            case DocAll:
                found = true;
                break;
            case NodeChildren:
                found = true;
                deep = false;
                break;
            default:
                break;
            }

            if (found)
                return current;
            if (deep) {
                current = current->traverseNextNode(m_base.get());
                continue;
            } 
        }
        current = current->traverseNextSibling(m_base.get());
    }
    return 0;
}


unsigned HTMLCollection::calcLength() const
{
    unsigned len = 0;

    for (Node *current = traverseNextItem(m_base.get()); current; current = traverseNextItem(current)) {
        len++;
    }

    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed
unsigned HTMLCollection::length() const
{
    resetCollectionInfo();
    if (!info->haslength) {
        info->length = calcLength();
        info->haslength = true;
    }
    return info->length;
}

Node *HTMLCollection::item( unsigned index ) const
{
     resetCollectionInfo();
     if (info->current && info->position == index) {
         return info->current;
     }
     if (info->haslength && info->length <= index) {
         return 0;
     }
     if (!info->current || info->position > index) {
         info->current = traverseNextItem(m_base.get());
         info->position = 0;
         if (!info->current)
             return 0;
     }
     Node *node = info->current;
     for (unsigned pos = info->position; node && pos < index; pos++) {
         node = traverseNextItem(node);
     }     
     info->current = node;
     info->position = index;
     return info->current;
}

Node *HTMLCollection::firstItem() const
{
     return item(0);
}

Node *HTMLCollection::nextItem() const
{
     resetCollectionInfo();
 
     // Look for the 'second' item. The first one is currentItem, already given back.
     Node *retval = traverseNextItem(info->current);
     info->current = retval;
     info->position++;
     return retval;
}

bool HTMLCollection::checkForNameMatch(Node *node, bool checkName, const String &name, bool caseSensitive) const
{
    if (!node->isHTMLElement())
        return false;
    
    HTMLElement *e = static_cast<HTMLElement*>(node);
    if (caseSensitive) {
        if (checkName) {
            // document.all returns only images, forms, applets, objects and embeds
            // by name (though everything by id)
            if (type == DocAll && 
                !(e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                  e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                  e->hasLocalName(embedTag) || e->hasLocalName(inputTag)))
                return false;

            return e->getAttribute(nameAttr) == name && e->getAttribute(idAttr) != name;
        } else
            return e->getAttribute(idAttr) == name;
    } else {
        if (checkName) {
            // document.all returns only images, forms, applets, objects and embeds
            // by name (though everything by id)
            if (type == DocAll && 
                !(e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                  e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                  e->hasLocalName(embedTag) || e->hasLocalName(inputTag)))
                return false;

            return e->getAttribute(nameAttr).domString().lower() == name.lower() &&
                e->getAttribute(idAttr).domString().lower() != name.lower();
        } else {
            return e->getAttribute(idAttr).domString().lower() == name.lower();
        }
    }
}


Node *HTMLCollection::namedItem(const String &name, bool caseSensitive) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    idsDone = false;

    Node *n;
    for (n = traverseNextItem(m_base.get()); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, caseSensitive))
            break;
    }
        
    info->current = n;
    if(info->current)
        return info->current;
    idsDone = true;

    for (n = traverseNextItem(m_base.get()); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, caseSensitive))
            break;
    }

    info->current = n;
    return info->current;
}

void HTMLCollection::updateNameCache() const
{
    if (info->hasNameCache)
        return;
    
    for (Node *n = traverseNextItem(m_base.get()); n; n = traverseNextItem(n)) {
        if (!n->isHTMLElement())
            continue;
        HTMLElement* e = static_cast<HTMLElement*>(n);
        const AtomicString& idAttrVal = e->getAttribute(idAttr);
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty()) {
            // add to id cache
            Vector<Node*>* idVector = info->idCache.get(idAttrVal.impl());
            if (!idVector) {
                idVector = new Vector<Node*>;
                info->idCache.add(idAttrVal.impl(), idVector);
            }
            idVector->append(n);
        }
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal
            && (type != DocAll || 
                (e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                 e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                 e->hasLocalName(embedTag) || e->hasLocalName(inputTag)))) {
            // add to name cache
            Vector<Node*>* nameVector = info->nameCache.get(nameAttrVal.impl());
            if (!nameVector) {
                nameVector = new Vector<Node*>;
                info->nameCache.add(nameAttrVal.impl(), nameVector);
            }
            nameVector->append(n);
        }
    }

    info->hasNameCache = true;
}

DeprecatedValueList< RefPtr<Node> > HTMLCollection::namedItems(const AtomicString &name) const
{
    DeprecatedValueList< RefPtr<Node> > result;

    if (name.isEmpty())
        return result;

    resetCollectionInfo();
    updateNameCache();
    
    Vector<Node*>* idResults = info->idCache.get(name.impl());
    Vector<Node*>* nameResults = info->nameCache.get(name.impl());
    
    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(RefPtr<Node>(idResults->at(i)));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(RefPtr<Node>(nameResults->at(i)));

    return result;
}


Node *HTMLCollection::nextNamedItem(const String &name) const
{
    resetCollectionInfo();

    for (Node *n = traverseNextItem(info->current ? info->current : m_base.get()); n; n = traverseNextItem(n)) {
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

    for (Node *n = traverseNextItem(info->current ? info->current : m_base.get()); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, true)) {
            info->current = n;
            return n;
        }
    }

    return 0;
}

}
