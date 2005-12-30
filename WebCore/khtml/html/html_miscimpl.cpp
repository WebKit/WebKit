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
#include "config.h"
#include "html/html_miscimpl.h"
#include "html/html_formimpl.h"
#include "html/html_imageimpl.h"
#include "html/html_documentimpl.h"
#include "html/html_objectimpl.h"

#include "dom/dom_node.h"

#include <kdebug.h>
#include <kxmlcore/HashSet.h>

namespace DOM {

using namespace HTMLNames;

HTMLBaseFontElementImpl::HTMLBaseFontElementImpl(DocumentImpl *doc)
    : HTMLElementImpl(basefontTag, doc)
{
}

DOMString HTMLBaseFontElementImpl::color() const
{
    return getAttribute(colorAttr);
}

void HTMLBaseFontElementImpl::setColor(const DOMString &value)
{
    setAttribute(colorAttr, value);
}

DOMString HTMLBaseFontElementImpl::face() const
{
    return getAttribute(faceAttr);
}

void HTMLBaseFontElementImpl::setFace(const DOMString &value)
{
    setAttribute(faceAttr, value);
}

DOMString HTMLBaseFontElementImpl::size() const
{
    return getAttribute(sizeAttr);
}

void HTMLBaseFontElementImpl::setSize(const DOMString &value)
{
    setAttribute(sizeAttr, value);
}

// -------------------------------------------------------------------------

HTMLCollectionImpl::HTMLCollectionImpl(NodeImpl *_base, int _type)
    : m_base(_base),
      type(_type),
      info(0),
      idsDone(false),
      m_ownsInfo(false)
{
    if (_base->isDocumentNode() && _base->getDocument()->isHTMLDocument())
        info = static_cast<HTMLDocumentImpl*>(_base->getDocument())->collectionInfo(type);
}

HTMLCollectionImpl::~HTMLCollectionImpl()
{
    if (m_ownsInfo)
        delete info;
}

HTMLCollectionImpl::CollectionInfo::CollectionInfo() :
    version(0)
{
    reset();
}

void HTMLCollectionImpl::CollectionInfo::reset()
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

void HTMLCollectionImpl::resetCollectionInfo() const
{
    unsigned int docversion = static_cast<HTMLDocumentImpl*>(m_base->getDocument())->domTreeVersion();

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


NodeImpl *HTMLCollectionImpl::traverseNextItem(NodeImpl *current) const
{
    assert(current);

    if (type == NODE_CHILDREN && m_base.get() != current)
        current = current->nextSibling();
    else
        current = current->traverseNextNode(m_base.get());

    while (current) {
        if (current->isElementNode()) {
            bool found = false;
            bool deep = true;
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(current);
            switch(type) {
            case DOC_IMAGES:
                if (e->hasLocalName(imgTag))
                    found = true;
                break;
            case DOC_FORMS:
                if(e->hasLocalName(formTag))
                    found = true;
                break;
            case TABLE_TBODIES:
                if (e->hasLocalName(tbodyTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case TR_CELLS:
                if (e->hasLocalName(tdTag) || e->hasLocalName(thTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case TABLE_ROWS:
            case TSECTION_ROWS:
                if (e->hasLocalName(trTag))
                    found = true;
                else if (e->hasLocalName(tableTag))
                    deep = false;
                break;
            case SELECT_OPTIONS:
                if (e->hasLocalName(optionTag))
                    found = true;
                break;
            case MAP_AREAS:
                if (e->hasLocalName(areaTag))
                    found = true;
                break;
            case DOC_APPLETS:   // all OBJECT and APPLET elements
                if (e->hasLocalName(objectTag) || e->hasLocalName(appletTag))
                    found = true;
                break;
            case DOC_EMBEDS:   // all EMBED elements
                if (e->hasLocalName(embedTag))
                    found = true;
                break;
            case DOC_OBJECTS:   // all OBJECT elements
                if (e->hasLocalName(objectTag))
                    found = true;
                break;
            case DOC_LINKS:     // all A _and_ AREA elements with a value for href
                if (e->hasLocalName(aTag) || e->hasLocalName(areaTag))
                    if (!e->getAttribute(hrefAttr).isNull())
                        found = true;
                break;
            case DOC_ANCHORS:      // all A elements with a value for name or an id attribute
                if (e->hasLocalName(aTag))
                    if (!e->getAttribute(nameAttr).isNull())
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


unsigned HTMLCollectionImpl::calcLength() const
{
    unsigned len = 0;

    for (NodeImpl *current = traverseNextItem(m_base.get()); current; current = traverseNextItem(current)) {
        len++;
    }

    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed
unsigned HTMLCollectionImpl::length() const
{
    resetCollectionInfo();
    if (!info->haslength) {
        info->length = calcLength();
        info->haslength = true;
    }
    return info->length;
}

NodeImpl *HTMLCollectionImpl::item( unsigned index ) const
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
     NodeImpl *node = info->current;
     for (unsigned pos = info->position; node && pos < index; pos++) {
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
    if (!node->isHTMLElement())
        return false;
    
    HTMLElementImpl *e = static_cast<HTMLElementImpl*>(node);
    if (caseSensitive) {
        if (checkName) {
            // document.all returns only images, forms, applets, objects and embeds
            // by name (though everything by id)
            if (type == DOC_ALL && 
                !(e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                  e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                  e->hasLocalName(embedTag)))
                return false;

            return e->getAttribute(nameAttr) == name && e->getAttribute(idAttr) != name;
        } else
            return e->getAttribute(idAttr) == name;
    } else {
        if (checkName) {
            // document.all returns only images, forms, applets, objects and embeds
            // by name (though everything by id)
            if (type == DOC_ALL && 
                !(e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                  e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                  e->hasLocalName(embedTag)))
                return false;

            return e->getAttribute(nameAttr).domString().lower() == name.lower() &&
                e->getAttribute(idAttr).domString().lower() != name.lower();
        } else {
            return e->getAttribute(idAttr).domString().lower() == name.lower();
        }
    }
}


NodeImpl *HTMLCollectionImpl::namedItem(const DOMString &name, bool caseSensitive) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    idsDone = false;

    NodeImpl *n;
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

HTMLNameCollectionImpl::HTMLNameCollectionImpl(DocumentImpl* _base, int _type, DOMString &name)
    : HTMLCollectionImpl(_base, _type),
      m_name(name)
{
}
    
NodeImpl *HTMLNameCollectionImpl::traverseNextItem(NodeImpl *current) const
{
    assert(current);

    current = current->traverseNextNode(m_base.get());

    while (current) {
        if (current->isElementNode()) {
            bool found = false;
            ElementImpl *e = static_cast<ElementImpl *>(current);
            switch(type) {
            case WINDOW_NAMED_ITEMS:
                // find only images, forms, applets, embeds and objects by name, 
                // but anything by id
                if (e->hasTagName(imgTag) ||
                    e->hasTagName(formTag) ||
                    e->hasTagName(appletTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(objectTag))
                    found = e->getAttribute(nameAttr) == m_name;
                found |= e->getAttribute(idAttr) == m_name;
                break;
            case DOCUMENT_NAMED_ITEMS:
                // find images, forms, applets, embeds, objects and iframes by name, 
                // but only applets and object by id (this strange rule matches IE)
                if (e->hasTagName(imgTag) ||
                    e->hasTagName(formTag) ||
                    e->hasTagName(embedTag) ||
                    e->hasTagName(iframeTag))
                    found = e->getAttribute(nameAttr) == m_name;
                else if (e->hasTagName(appletTag))
                    found = e->getAttribute(nameAttr) == m_name ||
                        e->getAttribute(idAttr) == m_name;
                else if (e->hasTagName(objectTag))
                    found = (e->getAttribute(nameAttr) == m_name || e->getAttribute(idAttr) == m_name) &&
                        static_cast<HTMLObjectElementImpl *>(e)->isDocNamedItem();
                break;
            default:
                assert(0);
            }

            if (found)
                return current;
        }
        current = current->traverseNextNode(m_base.get());
    }
    return 0;
}

template<class T> static void appendToVector(QPtrVector<T> *vec, T *item)
{
    unsigned size = vec->size();
    unsigned count = vec->count();
    if (size == count)
        vec->resize(size == 0 ? 8 : (int)(size * 1.5));
    vec->insert(count, item);
}

void HTMLCollectionImpl::updateNameCache() const
{
    if (info->hasNameCache)
        return;
    
    for (NodeImpl *n = traverseNextItem(m_base.get()); n; n = traverseNextItem(n)) {
        if (!n->isHTMLElement())
            continue;
        HTMLElementImpl* e = static_cast<HTMLElementImpl*>(n);
        const AtomicString& idAttrVal = e->getAttribute(idAttr);
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty()) {
            // add to id cache
            QPtrVector<NodeImpl> *idVector = info->idCache.get(idAttrVal.impl());
            if (!idVector) {
                idVector = new QPtrVector<NodeImpl>;
                info->idCache.add(idAttrVal.impl(), idVector);
            }
            appendToVector(idVector, n);
        }
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal
            && (type != DOC_ALL || 
                (e->hasLocalName(imgTag) || e->hasLocalName(formTag) ||
                 e->hasLocalName(appletTag) || e->hasLocalName(objectTag) ||
                 e->hasLocalName(embedTag)))) {
            // add to name cache
            QPtrVector<NodeImpl> *nameVector = info->nameCache.get(nameAttrVal.impl());
            if (!nameVector) {
                nameVector = new QPtrVector<NodeImpl>;
                info->nameCache.add(nameAttrVal.impl(), nameVector);
            }
            appendToVector(nameVector, n);
        }
    }

    info->hasNameCache = true;
}

QValueList< RefPtr<NodeImpl> > HTMLCollectionImpl::namedItems(const AtomicString &name) const
{
    QValueList< RefPtr<NodeImpl> > result;

    if (name.isEmpty())
        return result;

    resetCollectionInfo();
    updateNameCache();
    
    QPtrVector<NodeImpl> *idResults = info->idCache.get(name.impl());
    QPtrVector<NodeImpl> *nameResults = info->nameCache.get(name.impl());
    
    for (unsigned i = 0; idResults && i < idResults->count(); ++i) {
        result.append(RefPtr<NodeImpl>(idResults->at(i)));
    }

    for (unsigned i = 0; nameResults && i < nameResults->count(); ++i) {
        result.append(RefPtr<NodeImpl>(nameResults->at(i)));
    }

    return result;
}


NodeImpl *HTMLCollectionImpl::nextNamedItem(const DOMString &name) const
{
    resetCollectionInfo();

    for (NodeImpl *n = traverseNextItem(info->current ? info->current : m_base.get()); n; n = traverseNextItem(n)) {
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

    for (NodeImpl *n = traverseNextItem(info->current ? info->current : m_base.get()); n; n = traverseNextItem(n)) {
        if (checkForNameMatch(n, idsDone, name, true)) {
            info->current = n;
            return n;
        }
    }

    return 0;
}

// -----------------------------------------------------------------------------

HTMLFormCollectionImpl::HTMLFormCollectionImpl(NodeImpl* _base)
    : HTMLCollectionImpl(_base, 0)
{
    HTMLFormElementImpl *formBase = static_cast<HTMLFormElementImpl*>(m_base.get());
    if (!formBase->collectionInfo) {
        formBase->collectionInfo = new CollectionInfo();
    }
    info = formBase->collectionInfo;
}

HTMLFormCollectionImpl::~HTMLFormCollectionImpl()
{
}

unsigned HTMLFormCollectionImpl::calcLength() const
{
    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>(m_base.get())->formElements;

    int len = 0;
    for ( unsigned i = 0; i < l.count(); i++ )
        if ( l.at( i )->isEnumeratable() )
            ++len;

    return len;
}

NodeImpl *HTMLFormCollectionImpl::item(unsigned index) const
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
        info->elementsArrayPosition = 0;
    }

    QPtrVector<HTMLGenericFormElementImpl> &l = static_cast<HTMLFormElementImpl*>(m_base.get())->formElements;
    unsigned currentIndex = info->position;
    
    for (unsigned i = info->elementsArrayPosition; i < l.count(); i++) {
        if (l[i]->isEnumeratable() ) {
            if (index == currentIndex) {
                info->position = index;
                info->current = l[i];
                info->elementsArrayPosition = i;
                return l[i];
            }

            currentIndex++;
        }
    }

    return 0;
}

NodeImpl* HTMLFormCollectionImpl::getNamedItem(NodeImpl*, const QualifiedName& attrName, const DOMString& name, bool caseSensitive) const
{
    info->position = 0;
    return getNamedFormItem(attrName, name, 0, caseSensitive);
}

NodeImpl* HTMLFormCollectionImpl::getNamedFormItem(const QualifiedName& attrName, const DOMString& name, int duplicateNumber, bool caseSensitive) const
{
    if (m_base->isElementNode()) {
        HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(m_base.get());
        bool foundInputElements = false;
        if (baseElement->hasLocalName(formTag)) {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
            for (unsigned i = 0; i < f->formElements.count(); ++i) {
                HTMLGenericFormElementImpl* e = f->formElements[i];
                if (e->isEnumeratable()) {
                    bool found;
                    if (caseSensitive)
                        found = e->getAttribute(attrName) == name;
                    else
                        found = e->getAttribute(attrName).domString().lower() == name.lower();
                    if (found) {
                        foundInputElements = true;
                        if (!duplicateNumber)
                            return e;
                        --duplicateNumber;
                    }
                }
            }
        }

        if (!foundInputElements) {
            HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);

            for(unsigned i = 0; i < f->imgElements.count(); ++i)
            {
                HTMLImageElementImpl* e = f->imgElements[i];
                bool found;
                if (caseSensitive)
                    found = e->getAttribute(attrName) == name;
                else
                    found = e->getAttribute(attrName).domString().lower() == name.lower();
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

NodeImpl * HTMLFormCollectionImpl::nextNamedItemInternal(const DOMString &name) const
{
    NodeImpl *retval = getNamedFormItem( idsDone ? nameAttr : idAttr, name, ++info->position, true );
    if (retval)
        return retval;
    if (idsDone) // we're done
        return 0;
    // After doing id, do name
    idsDone = true;
    return getNamedItem(m_base->firstChild(), nameAttr, name, true);
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
    info->current = getNamedItem(m_base->firstChild(), idAttr, name, true);
    if(info->current)
        return info->current;
    idsDone = true;
    info->current = getNamedItem(m_base->firstChild(), nameAttr, name, true);
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
        if(impl->isElementNode())
        {
            HTMLElementImpl *e = static_cast<HTMLElementImpl *>(impl);
            ok = (e->getAttribute(idAttr) != name);
            if (!ok)
                impl = nextNamedItemInternal( name );
        } else // can't happen
            ok = true;
    }
    return impl;
}

void HTMLFormCollectionImpl::updateNameCache() const
{
    if (info->hasNameCache)
        return;

    HashSet<DOMStringImpl*, PointerHash<DOMStringImpl*> > foundInputElements;

    if (!m_base->hasTagName(formTag)) {
        info->hasNameCache = true;
        return;
    }

    HTMLElementImpl* baseElement = static_cast<HTMLElementImpl*>(m_base.get());

    HTMLFormElementImpl* f = static_cast<HTMLFormElementImpl*>(baseElement);
    for (unsigned i = 0; i < f->formElements.count(); ++i) {
        HTMLGenericFormElementImpl* e = f->formElements[i];
        if (e->isEnumeratable()) {
            const AtomicString& idAttrVal = e->getAttribute(idAttr);
            const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
            if (!idAttrVal.isEmpty()) {
                // add to id cache
                QPtrVector<NodeImpl> *idVector = info->idCache.get(idAttrVal.impl());
                if (!idVector) {
                    idVector = new QPtrVector<NodeImpl>;
                    info->idCache.add(idAttrVal.impl(), idVector);
                }
                appendToVector(idVector, static_cast<NodeImpl *>(e));
                foundInputElements.insert(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                // add to name cache
                QPtrVector<NodeImpl> *nameVector = info->nameCache.get(nameAttrVal.impl());
                if (!nameVector) {
                    nameVector = new QPtrVector<NodeImpl>;
                    info->nameCache.add(nameAttrVal.impl(), nameVector);
                }
                appendToVector(nameVector, static_cast<NodeImpl *>(e));
                foundInputElements.insert(nameAttrVal.impl());
            }
        }
    }

    for (unsigned i = 0; i < f->imgElements.count(); ++i) {
        HTMLImageElementImpl* e = f->imgElements[i];
        const AtomicString& idAttrVal = e->getAttribute(idAttr);
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl())) {
            // add to id cache
            QPtrVector<NodeImpl> *idVector = info->idCache.get(idAttrVal.impl());
            if (!idVector) {
                idVector = new QPtrVector<NodeImpl>;
                info->idCache.add(idAttrVal.impl(), idVector);
            }
            appendToVector(idVector, static_cast<NodeImpl *>(e));
        }
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl())) {
            // add to name cache
            QPtrVector<NodeImpl> *nameVector = info->nameCache.get(nameAttrVal.impl());
            if (!nameVector) {
                nameVector = new QPtrVector<NodeImpl>;
                info->nameCache.add(nameAttrVal.impl(), nameVector);
            }
            appendToVector(nameVector, static_cast<NodeImpl *>(e));
        }
    }

    info->hasNameCache = true;
}

}
