/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "HTMLFormControlsCollection.h"

#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormControlsCollection::HTMLFormControlsCollection(ContainerNode& ownerNode)
    : HTMLCollection(ownerNode, FormControls, CustomForwardOnlyTraversal)
    , m_cachedElement(nullptr)
    , m_cachedElementOffsetInArray(0)
{
    ASSERT(is<HTMLFormElement>(ownerNode) || is<HTMLFieldSetElement>(ownerNode));
}

Ref<HTMLFormControlsCollection> HTMLFormControlsCollection::create(ContainerNode& ownerNode, CollectionType)
{
    return adoptRef(*new HTMLFormControlsCollection(ownerNode));
}

HTMLFormControlsCollection::~HTMLFormControlsCollection()
{
}

const Vector<FormAssociatedElement*>& HTMLFormControlsCollection::formControlElements() const
{
    ASSERT(is<HTMLFormElement>(ownerNode()) || is<HTMLFieldSetElement>(ownerNode()));
    if (is<HTMLFormElement>(ownerNode()))
        return downcast<HTMLFormElement>(ownerNode()).associatedElements();
    return downcast<HTMLFieldSetElement>(ownerNode()).associatedElements();
}

const Vector<HTMLImageElement*>& HTMLFormControlsCollection::formImageElements() const
{
    ASSERT(is<HTMLFormElement>(ownerNode()));
    return downcast<HTMLFormElement>(ownerNode()).imageElements();
}

static unsigned findFormAssociatedElement(const Vector<FormAssociatedElement*>& elements, const Element& element)
{
    for (unsigned i = 0; i < elements.size(); ++i) {
        auto& associatedElement = *elements[i];
        if (associatedElement.isEnumeratable() && &associatedElement.asHTMLElement() == &element)
            return i;
    }
    return elements.size();
}

Element* HTMLFormControlsCollection::customElementAfter(Element* current) const
{
    const Vector<FormAssociatedElement*>& elements = formControlElements();
    unsigned start;
    if (!current)
        start = 0;
    else if (m_cachedElement == current)
        start = m_cachedElementOffsetInArray + 1;
    else
        start = findFormAssociatedElement(elements, *current) + 1;

    for (unsigned i = start; i < elements.size(); ++i) {
        FormAssociatedElement& element = *elements[i];
        if (element.isEnumeratable()) {
            m_cachedElement = &element.asHTMLElement();
            m_cachedElementOffsetInArray = i;
            return &element.asHTMLElement();
        }
    }
    return nullptr;
}

static HTMLElement* firstNamedItem(const Vector<FormAssociatedElement*>& elementsArray,
    const Vector<HTMLImageElement*>* imageElementsArray, const QualifiedName& attrName, const String& name)
{
    ASSERT(attrName == idAttr || attrName == nameAttr);

    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        HTMLElement& element = elementsArray[i]->asHTMLElement();
        if (elementsArray[i]->isEnumeratable() && element.fastGetAttribute(attrName) == name)
            return &element;
    }

    if (!imageElementsArray)
        return 0;

    for (unsigned i = 0; i < imageElementsArray->size(); ++i) {
        HTMLImageElement& element = *(*imageElementsArray)[i];
        if (element.fastGetAttribute(attrName) == name)
            return &element;
    }

    return nullptr;
}

HTMLElement* HTMLFormControlsCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    auto* imageElements = is<HTMLFieldSetElement>(ownerNode()) ? nullptr : &formImageElements();
    if (HTMLElement* item = firstNamedItem(formControlElements(), imageElements, idAttr, name))
        return item;
    return firstNamedItem(formControlElements(), imageElements, nameAttr, name);
}

void HTMLFormControlsCollection::updateNamedElementCache() const
{
    if (hasNamedElementCache())
        return;

    auto cache = std::make_unique<CollectionNamedElementCache>();

    bool ownerIsFormElement = is<HTMLFormElement>(ownerNode());
    HashSet<AtomicStringImpl*> foundInputElements;

    for (auto& elementPtr : formControlElements()) {
        FormAssociatedElement& associatedElement = *elementPtr;
        if (associatedElement.isEnumeratable()) {
            HTMLElement& element = associatedElement.asHTMLElement();
            const AtomicString& id = element.getIdAttribute();
            if (!id.isEmpty()) {
                cache->appendToIdCache(id, element);
                if (ownerIsFormElement)
                    foundInputElements.add(id.impl());
            }
            const AtomicString& name = element.getNameAttribute();
            if (!name.isEmpty() && id != name) {
                cache->appendToNameCache(name, element);
                if (ownerIsFormElement)
                    foundInputElements.add(name.impl());
            }
        }
    }
    if (ownerIsFormElement) {
        for (auto* elementPtr : formImageElements()) {
            HTMLImageElement& element = *elementPtr;
            const AtomicString& id = element.getIdAttribute();
            if (!id.isEmpty() && !foundInputElements.contains(id.impl()))
                cache->appendToIdCache(id, element);
            const AtomicString& name = element.getNameAttribute();
            if (!name.isEmpty() && id != name && !foundInputElements.contains(name.impl()))
                cache->appendToNameCache(name, element);
        }
    }

    setNamedItemCache(WTF::move(cache));
}

void HTMLFormControlsCollection::invalidateCache(Document& document) const
{
    HTMLCollection::invalidateCache(document);
    m_cachedElement = nullptr;
    m_cachedElementOffsetInArray = 0;
}

}
