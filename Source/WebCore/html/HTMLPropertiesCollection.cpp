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

namespace WebCore {

using namespace HTMLNames;

static inline bool compareTreeOrder(Node* node1, Node* node2)
{
    return (node2->compareDocumentPosition(node1) & (Node::DOCUMENT_POSITION_PRECEDING | Node::DOCUMENT_POSITION_DISCONNECTED)) == Node::DOCUMENT_POSITION_PRECEDING;
}

PassRefPtr<HTMLPropertiesCollection> HTMLPropertiesCollection::create(PassRefPtr<Node> itemNode)
{
    return adoptRef(new HTMLPropertiesCollection(itemNode));
}

HTMLPropertiesCollection::HTMLPropertiesCollection(PassRefPtr<Node> itemNode)
    : HTMLCollection(itemNode, ItemProperties)
    , m_propertyNames(DOMStringList::create())
{
}

HTMLPropertiesCollection::~HTMLPropertiesCollection()
{
}

void HTMLPropertiesCollection::findPropetiesOfAnItem(Node* root) const
{
    // 5.2.5 Associating names with items.
    Vector<Node*> memory;

    memory.append(root);

    Vector<Node*> pending;
    // Add the child elements of root, if any, to pending.
    for (Node* child = root->firstChild(); child; child = child->nextSibling())
        if (child->isHTMLElement())
            pending.append(child);

    // If root has an itemref attribute, split the value of that itemref attribute on spaces.
    // For each resulting token ID, if there is an element in the home subtree of root with the ID ID,
    // then add the first such element to pending.
    if (toHTMLElement(root)->fastHasAttribute(itemrefAttr)) {
        DOMSettableTokenList* itemRef = root->itemRef();

        for (size_t i = 0; i < itemRef->length(); ++i) {
            AtomicString id = itemRef->item(i);

            Element* element = root->document()->getElementById(id);
            if (element && element->isHTMLElement())
                pending.append(element);
        }
    }

    // Loop till we have processed all pending elements
    while (!pending.isEmpty()) {

        // Remove first element from pending and let current be that element.
        Node* current = pending[0];
        pending.remove(0);

        // If current is already in memory, there is a microdata error;
        if (memory.contains(current)) {
            // microdata error;
            continue;
        }

        memory.append(current);

        // If current does not have an itemscope attribute, then: add all the child elements of current to pending.
        HTMLElement* element = toHTMLElement(current);
        if (!element->fastHasAttribute(itemscopeAttr)) {
            for (Node* child = current->firstChild(); child; child = child->nextSibling())
                if (child->isHTMLElement())
                    pending.append(child);
        }

        // If current has an itemprop attribute specified, add it to results.
        if (element->fastHasAttribute(itempropAttr))
             m_properties.append(current);
    }
}

unsigned HTMLPropertiesCollection::length() const
{
    if (!base())
        return 0;

    if (!base()->isHTMLElement() || !toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return 0;

    m_properties.clear();
    findPropetiesOfAnItem(base());
    return m_properties.size();
}

Node* HTMLPropertiesCollection::item(unsigned index) const
{
    if (!base())
        return 0;

    if (!base()->isHTMLElement() || !toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return 0;

    m_properties.clear();
    findPropetiesOfAnItem(base());

    if (m_properties.size() <= index)
        return 0;

    std::sort(m_properties.begin(), m_properties.end(), compareTreeOrder);
    return m_properties[index];
}

PassRefPtr<DOMStringList> HTMLPropertiesCollection::names() const
{
    m_properties.clear();
    m_propertyNames->clear();

    if (!base())
        return 0;

    if (!base()->isHTMLElement() || !toHTMLElement(base())->fastHasAttribute(itemscopeAttr))
        return m_propertyNames;

    findPropetiesOfAnItem(base());

    std::sort(m_properties.begin(), m_properties.end(), compareTreeOrder);

    for (size_t i = 0; i < m_properties.size(); ++i) {
        // For each item properties, split the value of that itemprop attribute on spaces.
        // Add all tokens to property names, with the order preserved but with duplicates removed.
        DOMSettableTokenList* itemProperty = m_properties[i]->itemProp();
        for (size_t i = 0; i < itemProperty->length(); ++i) {
            AtomicString propertyName = itemProperty->item(i);
            if (m_propertyNames->isEmpty() || !m_propertyNames->contains(propertyName))
                m_propertyNames->append(propertyName);
        }
    }

    return m_propertyNames;
}

} // namespace WebCore

#endif // ENABLE(MICRODATA)
