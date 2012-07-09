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

static bool shouldIncludeChildren(CollectionType type)
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
    case TableRows:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case WindowNamedItems:
#if ENABLE(MICRODATA)
    case ItemProperties:
#endif
    case FormControls:
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

HTMLCollection::HTMLCollection(Node* base, CollectionType type)
    : HTMLCollectionCacheBase(type, shouldIncludeChildren(type))
    , m_base(base)
{
    ASSERT(m_base);
}

PassRefPtr<HTMLCollection> HTMLCollection::create(Node* base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type));
}

HTMLCollection::~HTMLCollection()
{
    if (isUnnamedDocumentCachedType(type())) {
        ASSERT(base()->isDocumentNode());
        static_cast<Document*>(base())->removeCachedHTMLCollection(this, type());
    } else if (isNodeCollectionType(type())) {
        ASSERT(base()->isElementNode());
        toElement(base())->removeCachedHTMLCollection(this, type());
    } else // HTMLNameCollection removes cache by itself.
        ASSERT(type() == WindowNamedItems || type() == DocumentNamedItems);
}

void HTMLCollection::invalidateCacheIfNeeded() const
{
    uint64_t docversion = static_cast<HTMLDocument*>(m_base->document())->domTreeVersion();

    if (cacheTreeVersion() == docversion)
        return;

    clearCache(docversion);
}

void HTMLCollection::invalidateCache()
{
    clearCache(static_cast<HTMLDocument*>(m_base->document())->domTreeVersion());
}

inline bool HTMLCollection::isAcceptableElement(Element* element) const
{
    if (!element->isHTMLElement() && !(type() == DocAll || type() == NodeChildren))
        return false;

    switch (type()) {
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
    case SelectedOptions:
        return element->hasLocalName(optionTag) && toHTMLOptionElement(element)->selected();
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
        return element->fastHasAttribute(itempropAttr);
#endif
    case FormControls:
    case DocumentNamedItems:
    case TableRows:
    case WindowNamedItems:
        ASSERT_NOT_REACHED();
    }
    return false;
}

static Node* nextNodeOrSibling(Node* base, Node* node, bool includeChildren)
{
    return includeChildren ? node->traverseNextNode(base) : node->traverseNextSibling(base);
}

Element* HTMLCollection::itemAfter(Node* previous) const
{
    Node* current;
    if (!previous)
        current = m_base->firstChild();
    else
        current = nextNodeOrSibling(base(), previous, includeChildren());

    for (; current; current = nextNodeOrSibling(base(), current, includeChildren())) {
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
    unsigned len = 0;
    for (Element* current = itemAfter(0); current; current = itemAfter(current))
        ++len;
    return len;
}

// since the collections are to be "live", we have to do the
// calculation every time if anything has changed
unsigned HTMLCollection::length() const
{
    invalidateCacheIfNeeded();
    if (!isLengthCacheValid())
        setLengthCache(calcLength());
    return cachedLength();
}

Node* HTMLCollection::item(unsigned index) const
{
    invalidateCacheIfNeeded();
    if (isItemCacheValid() && cachedItemOffset() == index)
        return cachedItem();
    if (isLengthCacheValid() && cachedLength() <= index)
        return 0;
    if (!isItemCacheValid() || cachedItemOffset() > index) {
        setItemCache(itemAfter(0), 0);
        if (!cachedItem())
            return 0;
    }
    Node* e = cachedItem();
    for (unsigned pos = cachedItemOffset(); e && pos < index; pos++)
        e = itemAfter(e);
    setItemCache(e, index);
    return cachedItem();
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

    if (type() == DocAll && !nameShouldBeVisibleInDocumentAll(e))
        return false;

    return e->getNameAttribute() == name && e->getIdAttribute() != name;
}

Node* HTMLCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    invalidateCacheIfNeeded();

    unsigned i = 0;
    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ false, name)) {
            setItemCache(e, i);
            return e;
        }
        i++;
    }

    i = 0;
    for (Element* e = itemAfter(0); e; e = itemAfter(e)) {
        if (checkForNameMatch(e, /* checkName */ true, name)) {
            setItemCache(e, i);
            return e;
        }
        i++;
    }

    return 0;
}

void HTMLCollection::updateNameCache() const
{
    if (hasNameCache())
        return;

    for (Element* element = itemAfter(0); element; element = itemAfter(element)) {
        if (!element->isHTMLElement())
            continue;
        HTMLElement* e = toHTMLElement(element);
        const AtomicString& idAttrVal = e->getIdAttribute();
        const AtomicString& nameAttrVal = e->getNameAttribute();
        if (!idAttrVal.isEmpty())
            appendIdCache(idAttrVal, e);
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && (type() != DocAll || nameShouldBeVisibleInDocumentAll(e)))
            appendNameCache(nameAttrVal, e);
    }

    setHasNameCache();
}

bool HTMLCollection::hasNamedItem(const AtomicString& name) const
{
    if (name.isEmpty())
        return false;

    invalidateCacheIfNeeded();
    updateNameCache();

    if (Vector<Element*>* cache = idCache(name)) {
        if (!cache->isEmpty())
            return true;
    }

    if (Vector<Element*>* cache = nameCache(name)) {
        if (!cache->isEmpty())
            return true;
    }

    return false;
}

void HTMLCollection::namedItems(const AtomicString& name, Vector<RefPtr<Node> >& result) const
{
    ASSERT(result.isEmpty());
    if (name.isEmpty())
        return;

    invalidateCacheIfNeeded();
    updateNameCache();

    Vector<Element*>* idResults = idCache(name);
    Vector<Element*>* nameResults = nameCache(name);

    for (unsigned i = 0; idResults && i < idResults->size(); ++i)
        result.append(idResults->at(i));

    for (unsigned i = 0; nameResults && i < nameResults->size(); ++i)
        result.append(nameResults->at(i));
}

PassRefPtr<NodeList> HTMLCollection::tags(const String& name)
{
    return m_base->getElementsByTagName(name);
}

void HTMLCollectionCacheBase::append(NodeCacheMap& map, const AtomicString& key, Element* element)
{
    OwnPtr<Vector<Element*> >& vector = map.add(key.impl(), nullptr).iterator->second;
    if (!vector)
        vector = adoptPtr(new Vector<Element*>);
    vector->append(element);
}

} // namespace WebCore
