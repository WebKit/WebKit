/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLCollection.h"

#include "CollectionCache.h"
#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include "NodeList.h"

#include <utility>

namespace WebCore {

using namespace HTMLNames;

HTMLCollection::HTMLCollection(Document* document, CollectionType type)
    : m_baseIsRetained(false)
    , m_includeChildren(shouldIncludeChildren(type))
    , m_ownsInfo(false)
    , m_type(type)
    , m_base(document)
    , m_info(0)
{
}

HTMLCollection::HTMLCollection(Node* base, CollectionType type, bool retainBaseNode)
    : m_baseIsRetained(retainBaseNode)
    , m_includeChildren(shouldIncludeChildren(type))
    , m_ownsInfo(false)
    , m_type(type)
    , m_base(base)
    , m_info(0)
{
    if (m_baseIsRetained)
        m_base->ref();
}

bool HTMLCollection::shouldIncludeChildren(CollectionType type)
{
    switch (type) {
    case DocAll:
    case DocAnchors:
    case DocApplets:
    case DocEmbeds:
    case DocForms:
    case DocImages:
    case DocLinks:
    case DocObjects:
    case DocScripts:
    case DocumentNamedItems:
    case MapAreas:
    case OtherCollection:
    case SelectOptions:
    case DataListOptions:
    case WindowNamedItems:
#if ENABLE(MICRODATA)
    case ItemProperties:
#endif
        return true;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

PassRefPtr<HTMLCollection> HTMLCollection::createForCachingOnDocument(Document* document, CollectionType type)
{
    return adoptRef(new HTMLCollection(document, type));
}

PassRefPtr<HTMLCollection> HTMLCollection::create(PassRefPtr<Node> base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base.get(), type));
}

HTMLCollection::~HTMLCollection()
{
    if (m_ownsInfo)
        delete m_info;
    if (m_baseIsRetained)
        m_base->deref();
}

void HTMLCollection::detachFromNode()
{
    m_base = 0;
    m_baseIsRetained = false;
}

void HTMLCollection::resetCollectionInfo() const
{
    ASSERT(m_base);

    uint64_t docversion = static_cast<HTMLDocument*>(m_base->document())->domTreeVersion();

    if (!m_info) {
        m_info = new CollectionCache;
        m_ownsInfo = true;
        m_info->version = docversion;
        return;
    }

    if (m_info->version != docversion) {
        m_info->reset();
        m_info->version = docversion;
    }
}

inline bool HTMLCollection::isAcceptableElement(Element* element) const
{
    switch (m_type) {
    case DocImages:
        return element->hasLocalName(imgTag);
    case DocScripts:
        return element->hasLocalName(scriptTag);
    case DocForms:
        return element->hasLocalName(formTag);
    case TableTBodies:
        return element->hasLocalName(tbodyTag);
    case TRCells:
        return element->hasLocalName(tdTag) || element->hasLocalName(thTag);
    case TSectionRows:
        return element->hasLocalName(trTag);
    case SelectOptions:
        return element->hasLocalName(optionTag);
    case DataListOptions:
        if (element->hasLocalName(optionTag)) {
            HTMLOptionElement* option = static_cast<HTMLOptionElement*>(element);
            if (!option->disabled() && !option->value().isEmpty())
                return true;
        }
        return false;
    case MapAreas:
        return element->hasLocalName(areaTag);
    case DocApplets:
        return element->hasLocalName(appletTag) || (element->hasLocalName(objectTag) && static_cast<HTMLObjectElement*>(element)->containsJavaApplet());
    case DocEmbeds:
        return element->hasLocalName(embedTag);
    case DocObjects:
        return element->hasLocalName(objectTag);
    case DocLinks:
        return (element->hasLocalName(aTag) || element->hasLocalName(areaTag)) && element->fastHasAttribute(hrefAttr);
    case DocAnchors:
        return element->hasLocalName(aTag) && element->fastHasAttribute(nameAttr);
    case DocAll:
    case NodeChildren:
        return true;
#if ENABLE(MICRODATA)
    case ItemProperties:
        return element->isHTMLElement() && element->fastHasAttribute(itempropAttr);
#endif
    case DocumentNamedItems:
    case OtherCollection:
    case WindowNamedItems:
        ASSERT_NOT_REACHED();
    }
    return false;
}

static Node* nextNodeOrSibling(Node* base, Node* node, bool includeChildren)
{
    return includeChildren ? node->traverseNextNode(base) : node->traverseNextSibling(base);
}

Element* HTMLCollection::itemAfter(Element* previous) const
{
    ASSERT(m_base);

    Node* current;
    if (!previous)
        current = m_base->firstChild();
    else
        current = nextNodeOrSibling(m_base, previous, m_includeChildren);

    for (; current; current = nextNodeOrSibling(m_base, current, m_includeChildren)) {
        if (!current->isElementNode())
            continue;
        Element* element = static_cast<Element*>(current);
        if (isAcceptableElement(element))
            return element;
    }

    return 0;
}

unsigned HTMLCollection::calcLength() const
{
    ASSERT(m_base);

    unsigned len = 0;
    for (Element* current = itemAfter(0); current; current = itemAfter(current))
        ++len;
    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed
unsigned HTMLCollection::length() const
{
    if (!m_base)
        return 0;

    resetCollectionInfo();
    if (!m_info->hasLength) {
        m_info->length = calcLength();
        m_info->hasLength = true;
    }
    return m_info->length;
}

Node* HTMLCollection::item(unsigned index) const
{
    if (!m_base)
        return 0;

     resetCollectionInfo();
     if (m_info->current && m_info->position == index)
         return m_info->current;
     if (m_info->hasLength && m_info->length <= index)
         return 0;
     if (!m_info->current || m_info->position > index) {
         m_info->current = itemAfter(0);
         m_info->position = 0;
         if (!m_info->current)
             return 0;
     }
     Element* e = m_info->current;
     for (unsigned pos = m_info->position; e && pos < index; pos++)
         e = itemAfter(e);
     m_info->current = e;
     m_info->position = index;
     return m_info->current;
}

Node* HTMLCollection::firstItem() const
{
     return item(0);
}

Node* HTMLCollection::nextItem() const
{
     ASSERT(m_base);
     resetCollectionInfo();

     // Look for the 'second' item. The first one is currentItem, already given back.
     Element* retval = itemAfter(m_info->current);
     m_info->current = retval;
     m_info->position++;
     return retval;
}

static inline bool nameShouldBeVisibleInDocumentAll(HTMLElement* element)
{
    // The document.all collection returns only certain types of elements by name,
    // although it returns any type of element by id.
    return element->hasLocalName(appletTag)
        || element->hasLocalName(embedTag)
        || element->hasLocalName(formTag)
        || element->hasLocalName(imgTag)
        || element->hasLocalName(inputTag)
        || element->hasLocalName(objectTag)
        || element->hasLocalName(selectTag);
}

bool HTMLCollection::checkForNameMatch(Element* element, bool checkName, const AtomicString& name) const
{
    if (!element->isHTMLElement())
        return false;
    
    HTMLElement* e = toHTMLElement(element);
    if (!checkName)
        return e->getIdAttribute() == name;

    if (m_type == DocAll && !nameShouldBeVisibleInDocumentAll(e))
        return false;

    return e->getAttribute(nameAttr) == name && e->getIdAttribute() != name;
}

Node* HTMLCollection::namedItem(const AtomicString& name) const
{
    if (!m_base)
        return 0;

    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();

    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ false, name)) {
            m_info->current = e;
            return e;
        }
    }

    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ true, name)) {
            m_info->current = e;
            return e;
        }
    }

    m_info->current = 0;
    return 0;
}

void HTMLCollection::updateNameCache() const
{
    ASSERT(m_base);

    if (m_info->hasNameCache)
        return;

    for (Element* element = itemAfter(0); element; element = itemAfter(element)) {
        if (!element->isHTMLElement())
            continue;
        HTMLElement* e = toHTMLElement(element);
        const AtomicString& idAttrVal = e->getIdAttribute();
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty())
            append(m_info->idCache, idAttrVal, e);
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (m_type != DocAll || nameShouldBeVisibleInDocumentAll(e)))
            append(m_info->nameCache, nameAttrVal, e);
    }

    m_info->hasNameCache = true;
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    if (!m_base)
        return false;

    if (name.isEmpty())
        return false;

    resetCollectionInfo();
    updateNameCache();
    m_info->checkConsistency();

    if (Vector<Element*>* idCache = m_info->idCache.get(name.impl())) {
        if (!idCache->isEmpty())
            return true;
    }

    if (Vector<Element*>* nameCache = m_info->nameCache.get(name.impl())) {
        if (!nameCache->isEmpty())
            return true;
    }

    return false;
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<RefPtr<Node> >& result) const
{
    if (!m_base)
        return;

    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    resetCollectionInfo();
    updateNameCache();
    m_info->checkConsistency();

    Vector<Element*>* idResults = m_info->idCache.get(name.impl());
    Vector<Element*>* nameResults = m_info->nameCache.get(name.impl());
    
    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(nameResults->at(i));
}

PassRefPtr<NodeList> HTMLCollection::tags(const String& name)
{
    if (!m_base)
        return 0;

    return m_base->getElementsByTagName(name);
}

} // namespace WebCore
