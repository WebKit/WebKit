/*
 * Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * Neither the name of Motorola Mobility, Inc. nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(MICRODATA)

#include "HTMLPropertiesCollection.h"

#include "DOMSettableTokenList.h"
#include "DOMStringList.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "Node.h"
#include "StaticNodeList.h"

namespace WebCore {

using namespace HTMLNames;

PassOwnPtr<HTMLPropertiesCollection> HTMLPropertiesCollection::create(Node* itemNode)
{
    return adoptPtr(new HTMLPropertiesCollection(itemNode));
}

HTMLPropertiesCollection::HTMLPropertiesCollection(Node* itemNode)
    : HTMLCollection(itemNode, ItemProperties)
{
}

HTMLPropertiesCollection::~HTMLPropertiesCollection()
{
}

void HTMLPropertiesCollection::invalidateCacheIfNeeded() const
{
    uint64_t docversion = base()->document()->domTreeVersion();

    if (m_cache.version == docversion)
        return;

    m_cache.clear();
    m_cache.version = docversion;
}

void HTMLPropertiesCollection::updateRefElements() const
{
    if (m_cache.hasItemRefElements)
        return;

    Vector<Element*> itemRefElements;
    HTMLElement* baseElement = toHTMLElement(base());

    if (!baseElement->fastHasAttribute(itemrefAttr)) {
        itemRefElements.append(baseElement);
        m_cache.setItemRefElements(itemRefElements);
        return;
    }

    DOMSettableTokenList* itemRef = baseElement->itemRef();
    RefPtr<DOMSettableTokenList> processedItemRef = DOMSettableTokenList::create();
    Node* rootNode = baseElement->treeScope()->rootNode();

    for (Node* current = rootNode->firstChild(); current; current = current->traverseNextNode(rootNode)) {
        if (!current->isHTMLElement())
            continue;
        HTMLElement* element = toHTMLElement(current);

        if (element == baseElement) {
            itemRefElements.append(element);
            continue;
        }

        const AtomicString& id = element->getIdAttribute();
        if (!processedItemRef->tokens().contains(id) && itemRef->tokens().contains(id)) {
            processedItemRef->setValue(id);
            if (!element->isDescendantOf(baseElement))
                itemRefElements.append(element);
        }
    }

    m_cache.setItemRefElements(itemRefElements);
}

static Node* nextNodeWithProperty(Node* base, Node* node)
{
    // An Microdata item may contain properties which in turn are items themselves. Properties can
    // also themselves be groups of name-value pairs, by putting the itemscope attribute on the element
    // that declares the property. If the property has an itemscope attribute specified then we need
    // to traverse the next sibling.
    return node == base || (node->isHTMLElement() && !toHTMLElement(node)->fastHasAttribute(itemscopeAttr))
            ? node->traverseNextNode(base) : node->traverseNextSibling(base);
}

Element* HTMLPropertiesCollection::itemAfter(Element* base, Element* previous) const
{
    Node* current;
    current = previous ? nextNodeWithProperty(base, previous) : base;

    for (; current; current = nextNodeWithProperty(base, current)) {
        if (!current->isHTMLElement())
            continue;
        HTMLElement* element = toHTMLElement(current);
        if (element->fastHasAttribute(itempropAttr)) {
            return element;
        }
    }

    return 0;
}

unsigned HTMLPropertiesCollection::calcLength() const
{
    unsigned length = 0;
    updateRefElements();

    const Vector<Element*>& itemRefElements = m_cache.getItemRefElements();
    for (unsigned i = 0; i < itemRefElements.size(); ++i) {
        for (Element* element = itemAfter(itemRefElements[i], 0); element; element = itemAfter(itemRefElements[i], element))
            ++length;
    }

    return length;
}

unsigned HTMLPropertiesCollection::length() const
{
    if (!toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return 0;

    invalidateCacheIfNeeded();

    if (!m_cache.hasLength)
        m_cache.updateLength(calcLength());

    return m_cache.length;
}

Element* HTMLPropertiesCollection::firstProperty() const
{
    Element* element = 0;
    m_cache.resetPosition();
    const Vector<Element*>& itemRefElements = m_cache.getItemRefElements();
    for (unsigned i = 0; i < itemRefElements.size(); ++i) {
        element = itemAfter(itemRefElements[i], 0);
        if (element) {
            m_cache.itemRefElementPosition = i;
            break;
        }
    }

    return element;
}

Node* HTMLPropertiesCollection::item(unsigned index) const
{
    if (!toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return 0;

    invalidateCacheIfNeeded();
    if (m_cache.current && m_cache.position == index)
        return m_cache.current;

    if (m_cache.hasLength && m_cache.length <= index)
        return 0;

    updateRefElements();
    if (!m_cache.current || m_cache.position > index) {
        m_cache.current = firstProperty();
        if (!m_cache.current)
            return 0;
    }

    unsigned currentPosition = m_cache.position;
    Element* element = m_cache.current;
    unsigned itemRefElementPos = m_cache.itemRefElementPosition;
    const Vector<Element*>& itemRefElements = m_cache.getItemRefElements();

    bool found = (m_cache.position == index);

    for (unsigned i = itemRefElementPos; i < itemRefElements.size() && !found; ++i) {
        while (currentPosition < index) {
            element = itemAfter(itemRefElements[i], element);
            if (!element)
                break;
            currentPosition++;

            if (currentPosition == index) {
                found = true;
                itemRefElementPos = i;
                break;
            }
        }
    }

    m_cache.updateCurrentItem(element, index, itemRefElementPos);
    return m_cache.current;
}

void HTMLPropertiesCollection::findProperties(Element* base) const
{
    for (Element* element = itemAfter(base, 0); element; element = itemAfter(base, element)) {
        DOMSettableTokenList* itemProperty = element->itemProp();
        for (unsigned i = 0; i < itemProperty->length(); ++i)
            m_cache.updatePropertyCache(element, itemProperty->item(i));
    }
}

void HTMLPropertiesCollection::updateNameCache() const
{
    invalidateCacheIfNeeded();
    if (m_cache.hasNameCache)
        return;

    updateRefElements();

    const Vector<Element*>& itemRefElements = m_cache.getItemRefElements();
    for (unsigned i = 0; i < itemRefElements.size(); ++i)
        findProperties(itemRefElements[i]);

    m_cache.hasNameCache = true;
}

PassRefPtr<DOMStringList> HTMLPropertiesCollection::names() const
{
    if (!toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return DOMStringList::create();

    updateNameCache();

    return m_cache.propertyNames;
}

PassRefPtr<NodeList> HTMLPropertiesCollection::namedItem(const String& name) const
{
    if (!toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
      return 0;

    Vector<RefPtr<Node> > namedItems;

    updateNameCache();

    Vector<Element*>* propertyResults = m_cache.propertyCache.get(AtomicString(name).impl());
    for (unsigned i = 0; propertyResults && i < propertyResults->size(); ++i)
        namedItems.append(propertyResults->at(i));

    // FIXME: HTML5 specifies that this should return PropertyNodeList.
    return namedItems.isEmpty() ? 0 : StaticNodeList::adopt(namedItems);
}

bool HTMLPropertiesCollection::hasNamedItem(const AtomicString& name) const
{
    if (!toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return false;

    updateNameCache();

    if (Vector<Element*>* propertyCache = m_cache.propertyCache.get(name.impl())) {
        if (!propertyCache->isEmpty())
            return true;
    }

    return false;
}

} // namespace WebCore

#endif // ENABLE(MICRODATA)
