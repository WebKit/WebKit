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

#if ENABLE(MICRODATA)
#include "HTMLPropertiesCollection.h"
#endif

#include <utility>

namespace WebCore {

using namespace HTMLNames;

static bool shouldOnlyIncludeDirectChildren(CollectionType type)
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
        return false;
    case NodeChildren:
    case TRCells:
    case TSectionRows:
    case TableTBodies:
        return true;
    case InvalidCollectionType:
        break;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static NodeListRootType rootTypeFromCollectionType(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocApplets:
    case DocEmbeds:
    case DocObjects:
    case DocForms:
    case DocLinks:
    case DocAnchors:
    case DocScripts:
    case DocAll:
    case WindowNamedItems:
    case DocumentNamedItems:
#if ENABLE(MICRODATA)
    case ItemProperties:
#endif
    case FormControls:
        return NodeListIsRootedAtDocument;
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case SelectedOptions:
    case DataListOptions:
    case MapAreas:
    case InvalidCollectionType:
        return NodeListIsRootedAtNode;
    }
    ASSERT_NOT_REACHED();
    return NodeListIsRootedAtNode;
}

static NodeListInvalidationType invalidationTypeExcludingIdAndNameAttributes(CollectionType type)
{
    switch (type) {
    case DocImages:
    case DocEmbeds:
    case DocObjects:
    case DocForms:
    case DocAnchors: // Depends on name attribute.
    case DocScripts:
    case DocAll:
    case WindowNamedItems: // Depends on id and name attributes.
    case DocumentNamedItems: // Ditto.
    case NodeChildren:
    case TableTBodies:
    case TSectionRows:
    case TableRows:
    case TRCells:
    case SelectOptions:
    case MapAreas:
        return DoNotInvalidateOnAttributeChanges;
    case DocApplets:
    case SelectedOptions:
    case DataListOptions:
        // FIXME: We can do better some day.
        return InvalidateOnAnyAttrChange;
    case DocLinks:
        return InvalidateOnHRefAttrChange;
#if ENABLE(MICRODATA)
    case ItemProperties:
        return InvalidateOnItemAttrChange;
#endif
    case FormControls:
        return InvalidateForFormControls;
    case InvalidCollectionType:
        break;
    }
    ASSERT_NOT_REACHED();
    return DoNotInvalidateOnAttributeChanges;
}
    

HTMLCollection::HTMLCollection(Node* base, CollectionType type, ItemBeforeSupportType itemBeforeSupportType)
    : HTMLCollectionCacheBase(rootTypeFromCollectionType(type), invalidationTypeExcludingIdAndNameAttributes(type), type, itemBeforeSupportType)
    , m_base(base)
{
    ASSERT(m_base);
    m_base->document()->registerNodeListCache(this);
}

PassRefPtr<HTMLCollection> HTMLCollection::create(Node* base, CollectionType type)
{
    return adoptRef(new HTMLCollection(base, type, SupportItemBefore));
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

    m_base->document()->unregisterNodeListCache(this);
}

static inline bool isAcceptableElement(CollectionType type, Element* element)
{
    if (!element->isHTMLElement() && !(type == DocAll || type == NodeChildren))
        return false;

    switch (type) {
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
    case InvalidCollectionType:
        ASSERT_NOT_REACHED();
    }
    return false;
}

template<bool forward>
static Node* nextNode(Node* base, Node* previous, bool onlyIncludeDirectChildren)
{
    if (forward)
        return onlyIncludeDirectChildren ? previous->nextSibling() : previous->traverseNextNode(base);
    else
        return onlyIncludeDirectChildren ? previous->previousSibling() : previous->traversePreviousNode(base);
}

static inline Node* lastDescendent(Node* node)
{
    node = node->lastChild();
    for (Node* current = node; current; current = current->lastChild())
        node = current;
    return node;
}

template<bool forward>
static Element* itemBeforeOrAfter(CollectionType type, Node* base, unsigned& offsetInArray, Node* previous)
{
    ASSERT_UNUSED(offsetInArray, !offsetInArray);
    bool onlyIncludeDirectChildren = shouldOnlyIncludeDirectChildren(type);
    Node* rootNode = base;
    Node* current;
    if (previous)
        current = nextNode<forward>(rootNode, previous, onlyIncludeDirectChildren);
    else {
        if (forward)
            current = rootNode->firstChild();
        else
            current = onlyIncludeDirectChildren ? rootNode->lastChild() : lastDescendent(rootNode);
    }

    for (; current; current = nextNode<forward>(rootNode, current, onlyIncludeDirectChildren)) {
        if (current->isElementNode() && isAcceptableElement(type, toElement(current)))
            return toElement(current);
    }

    return 0;
}

Element* HTMLCollection::itemBefore(unsigned& offsetInArray, Element* previous) const
{
    return itemBeforeOrAfter<false>(type(), base(), offsetInArray, previous);
}

Element* HTMLCollection::itemAfter(unsigned& offsetInArray, Element* previous) const
{
    return itemBeforeOrAfter<true>(type(), base(), offsetInArray, previous);
}

bool ALWAYS_INLINE HTMLCollection::isLastItemCloserThanLastOrCachedItem(unsigned offset) const
{
    ASSERT(isLengthCacheValid());
    unsigned distanceFromLastItem = cachedLength() - offset;
    if (!isItemCacheValid())
        return distanceFromLastItem < offset;

    return cachedItemOffset() < offset && distanceFromLastItem < offset - cachedItemOffset();
}
    
bool ALWAYS_INLINE HTMLCollection::isFirstItemCloserThanCachedItem(unsigned offset) const
{
    ASSERT(isItemCacheValid());
    if (cachedItemOffset() < offset)
        return false;

    unsigned distanceFromCachedItem = cachedItemOffset() - offset;
    return offset < distanceFromCachedItem;
}

unsigned HTMLCollection::length() const
{
    if (isLengthCacheValid())
        return cachedLength();

    if (!isItemCacheValid() && !item(0)) {
        ASSERT(isLengthCacheValid());
        return 0;
    }

    ASSERT(isItemCacheValid());
    ASSERT(cachedItem());
    unsigned offset = cachedItemOffset();
    do {
        offset++;
    } while (itemBeforeOrAfterCachedItem(offset));
    ASSERT(isLengthCacheValid());

    return offset;
}

Node* HTMLCollection::item(unsigned offset) const
{
    if (isItemCacheValid() && cachedItemOffset() == offset)
        return cachedItem();

    if (isLengthCacheValid() && cachedLength() <= offset)
        return 0;

#if ENABLE(MICRODATA)
    if (type() == ItemProperties)
        static_cast<const HTMLPropertiesCollection*>(this)->updateRefElements();
#endif

    if (isLengthCacheValid() && supportsItemBefore() && isLastItemCloserThanLastOrCachedItem(offset)) {
        // FIXME: Need to figure out the last offset in array for HTMLFormCollection and HTMLPropertiesCollection
        unsigned unusedOffsetInArray = 0;
        Node* lastItem = itemBefore(unusedOffsetInArray, 0);
        ASSERT(!unusedOffsetInArray);
        ASSERT(lastItem);
        setItemCache(lastItem, cachedLength() - 1, 0);
    } else if (!isItemCacheValid() || isFirstItemCloserThanCachedItem(offset) || (!supportsItemBefore() && offset < cachedItemOffset())) {
        unsigned offsetInArray = 0;
        Node* firstItem = itemAfter(offsetInArray, 0);
        if (!firstItem) {
            setLengthCache(0);
            return 0;
        }
        setItemCache(firstItem, 0, offsetInArray);
        ASSERT(!cachedItemOffset());
    }

    if (cachedItemOffset() == offset)
        return cachedItem();

    return itemBeforeOrAfterCachedItem(offset);
}

Element* HTMLCollection::itemBeforeOrAfterCachedItem(unsigned offset) const
{
    unsigned currentOffset = cachedItemOffset();
    ASSERT(cachedItem()->isElementNode());
    Element* currentItem = toElement(cachedItem());
    ASSERT(currentOffset != offset);

    unsigned offsetInArray = cachedElementsArrayOffset();

    if (offset < cachedItemOffset()) {
        ASSERT(supportsItemBefore());
        while ((currentItem = itemBefore(offsetInArray, currentItem))) {
            ASSERT(currentOffset);
            currentOffset--;
            if (currentOffset == offset) {
                setItemCache(currentItem, currentOffset, offsetInArray);
                return currentItem;
            }
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    while ((currentItem = itemAfter(offsetInArray, currentItem))) {
        currentOffset++;
        if (currentOffset == offset) {
            setItemCache(currentItem, currentOffset, offsetInArray);
            return currentItem;
        }
    }

    unsigned offsetOfLastItem = currentOffset;
    setLengthCache(offsetOfLastItem + 1);

    return 0;
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

    unsigned arrayOffset = 0;
    unsigned i = 0;
    for (Element* e = itemAfter(arrayOffset, 0); e; e = itemAfter(arrayOffset, e)) {
        if (checkForNameMatch(e, /* checkName */ false, name)) {
            setItemCache(e, i, arrayOffset);
            return e;
        }
        i++;
    }

    i = 0;
    for (Element* e = itemAfter(arrayOffset, 0); e; e = itemAfter(arrayOffset, e)) {
        if (checkForNameMatch(e, /* checkName */ true, name)) {
            setItemCache(e, i, arrayOffset);
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

    unsigned arrayOffset = 0;
    for (Element* element = itemAfter(arrayOffset, 0); element; element = itemAfter(arrayOffset, element)) {
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
