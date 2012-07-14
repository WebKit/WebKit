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

PassRefPtr<HTMLPropertiesCollection> HTMLPropertiesCollection::create(Node* itemNode)
{
    return adoptRef(new HTMLPropertiesCollection(itemNode));
}

HTMLPropertiesCollection::HTMLPropertiesCollection(Node* itemNode)
    : HTMLCollection(itemNode, ItemProperties, DoNotSupportItemBefore)
    , m_hasPropertyNameCache(false)
    , m_hasItemRefElements(false)
{
}

HTMLPropertiesCollection::~HTMLPropertiesCollection()
{
}

void HTMLPropertiesCollection::updateRefElements() const
{
    if (m_hasItemRefElements)
        return;

    HTMLElement* baseElement = toHTMLElement(base());

    m_itemRefElements.clear();
    m_hasItemRefElements = true;

    if (!baseElement->fastHasAttribute(itemscopeAttr))
        return;

    if (!baseElement->fastHasAttribute(itemrefAttr)) {
        m_itemRefElements.append(baseElement);
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
            m_itemRefElements.append(element);
            continue;
        }

        const AtomicString& id = element->getIdAttribute();
        if (!processedItemRef->tokens().contains(id) && itemRef->tokens().contains(id)) {
            processedItemRef->setValue(id);
            if (!element->isDescendantOf(baseElement))
                m_itemRefElements.append(element);
        }
    }
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

Element* HTMLPropertiesCollection::itemAfter(unsigned& offsetInArray, Element* previousItem) const
{
    while (offsetInArray < m_itemRefElements.size()) {
        if (Element* next = itemAfter(m_itemRefElements[offsetInArray], previousItem))
            return next;
        offsetInArray++;
        previousItem = 0;
    }
    return 0;
}

HTMLElement* HTMLPropertiesCollection::itemAfter(HTMLElement* base, Element* previous) const
{
    Node* current;
    current = previous ? nextNodeWithProperty(base, previous) : base;

    for (; current; current = nextNodeWithProperty(base, current)) {
        if (!current->isHTMLElement())
            continue;
        HTMLElement* element = toHTMLElement(current);
        if (element->fastHasAttribute(itempropAttr) && element->itemProp()->length()) {
            return element;
        }
    }

    return 0;
}

void HTMLPropertiesCollection::updateNameCache() const
{
    if (m_hasPropertyNameCache)
        return;

    updateRefElements();

    for (unsigned i = 0; i < m_itemRefElements.size(); ++i) {
        HTMLElement* refElement = m_itemRefElements[i];
        for (HTMLElement* element = itemAfter(refElement, 0); element; element = itemAfter(refElement, element)) {
            DOMSettableTokenList* itemProperty = element->itemProp();
            for (unsigned propertyIndex = 0; propertyIndex < itemProperty->length(); ++propertyIndex)
                updatePropertyCache(element, itemProperty->item(propertyIndex));
        }
    }

    m_hasPropertyNameCache = true;
}

PassRefPtr<DOMStringList> HTMLPropertiesCollection::names() const
{
    updateNameCache();
    if (!m_propertyNames)
        m_propertyNames = DOMStringList::create();
    return m_propertyNames;
}

PassRefPtr<NodeList> HTMLPropertiesCollection::namedItem(const String& name) const
{
    updateNameCache();

    Vector<RefPtr<Node> > namedItems;
    Vector<Element*>* propertyResults = m_propertyCache.get(AtomicString(name).impl());
    for (unsigned i = 0; propertyResults && i < propertyResults->size(); ++i)
        namedItems.append(propertyResults->at(i));

    // FIXME: HTML5 specifies that this should return PropertyNodeList.
    return namedItems.isEmpty() ? 0 : StaticNodeList::adopt(namedItems);
}

bool HTMLPropertiesCollection::hasNamedItem(const AtomicString& name) const
{
    updateNameCache();

    if (Vector<Element*>* propertyCache = m_propertyCache.get(name.impl())) {
        if (!propertyCache->isEmpty())
            return true;
    }

    return false;
}

} // namespace WebCore

#endif // ENABLE(MICRODATA)
