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

#include "HTMLDocument.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLOptionElement.h"
#include "NodeList.h"

#include <utility>

namespace WebCore {

using namespace HTMLNames;

HTMLCollection::HTMLCollection(Node* base, CollectionType type)
    : m_includeChildren(shouldIncludeChildren(type))
    , m_type(type)
    , m_base(base)
{
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

PassRefPtr<HTMLCollection> HTMLCollection::create(Node* base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type));
}

HTMLCollection::~HTMLCollection()
{
}

void HTMLCollection::detachFromNode()
{
    m_base = 0;
}

void HTMLCollection::invalidateCacheIfNeeded() const
{
    ASSERT(m_base);

    uint64_t docversion = static_cast<HTMLDocument*>(m_base->document())->domTreeVersion();

    if (m_cache.version == docversion)
        return;

    m_cache.current = 0;
    m_cache.position = 0;
    m_cache.length = 0;
    m_cache.hasLength = false;
    m_cache.elementsArrayPosition = 0;
    m_cache.idCache.clear();
    m_cache.nameCache.clear();
    m_cache.hasNameCache = false;
    m_cache.version = docversion;
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

    invalidateCacheIfNeeded();
    if (!m_cache.hasLength) {
        m_cache.length = calcLength();
        m_cache.hasLength = true;
    }
    return m_cache.length;
}

Node* HTMLCollection::item(unsigned index) const
{
    if (!m_base)
        return 0;

     invalidateCacheIfNeeded();
     if (m_cache.current && m_cache.position == index)
         return m_cache.current;
     if (m_cache.hasLength && m_cache.length <= index)
         return 0;
     if (!m_cache.current || m_cache.position > index) {
         m_cache.current = itemAfter(0);
         m_cache.position = 0;
         if (!m_cache.current)
             return 0;
     }
     Element* e = m_cache.current;
     for (unsigned pos = m_cache.position; e && pos < index; pos++)
         e = itemAfter(e);
     m_cache.current = e;
     m_cache.position = index;
     return m_cache.current;
}

Node* HTMLCollection::firstItem() const
{
     return item(0);
}

Node* HTMLCollection::nextItem() const
{
     ASSERT(m_base);
     invalidateCacheIfNeeded();

     // Look for the 'second' item. The first one is currentItem, already given back.
     Element* retval = itemAfter(m_cache.current);
     m_cache.current = retval;
     m_cache.position++;
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
    invalidateCacheIfNeeded();

    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ false, name)) {
            m_cache.current = e;
            return e;
        }
    }

    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ true, name)) {
            m_cache.current = e;
            return e;
        }
    }

    m_cache.current = 0;
    return 0;
}

void HTMLCollection::updateNameCache() const
{
    ASSERT(m_base);

    if (m_cache.hasNameCache)
        return;

    for (Element* element = itemAfter(0); element; element = itemAfter(element)) {
        if (!element->isHTMLElement())
            continue;
        HTMLElement* e = toHTMLElement(element);
        const AtomicString& idAttrVal = e->getIdAttribute();
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty())
            append(m_cache.idCache, idAttrVal, e);
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (m_type != DocAll || nameShouldBeVisibleInDocumentAll(e)))
            append(m_cache.nameCache, nameAttrVal, e);
    }

    m_cache.hasNameCache = true;
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    if (!m_base)
        return false;

    if (name.isEmpty())
        return false;

    invalidateCacheIfNeeded();
    updateNameCache();

    if (Vector<Element*>* idCache = m_cache.idCache.get(name.impl())) {
        if (!idCache->isEmpty())
            return true;
    }

    if (Vector<Element*>* nameCache = m_cache.nameCache.get(name.impl())) {
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

    invalidateCacheIfNeeded();
    updateNameCache();

    Vector<Element*>* idResults = m_cache.idCache.get(name.impl());
    Vector<Element*>* nameResults = m_cache.nameCache.get(name.impl());

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

void HTMLCollection::append(NodeCacheMap& map, const AtomicString& key, Element* element)
{
    OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).first->second;
    if (!vector)
        vector = adoptPtr(new Vector<Element*>);
    vector->append(element);
}

} // namespace WebCore
