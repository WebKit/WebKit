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
    ASSERT(isHTMLFormElement(ownerNode) || isHTMLFieldSetElement(ownerNode));
}

PassRefPtr<HTMLFormControlsCollection> HTMLFormControlsCollection::create(ContainerNode& ownerNode, CollectionType)
{
    return adoptRef(new HTMLFormControlsCollection(ownerNode));
}

HTMLFormControlsCollection::~HTMLFormControlsCollection()
{
}

const Vector<FormAssociatedElement*>& HTMLFormControlsCollection::formControlElements() const
{
    ASSERT(isHTMLFormElement(ownerNode()) || ownerNode().hasTagName(fieldsetTag));
    if (isHTMLFormElement(ownerNode()))
        return toHTMLFormElement(ownerNode()).associatedElements();
    return toHTMLFieldSetElement(ownerNode()).associatedElements();
}

const Vector<HTMLImageElement*>& HTMLFormControlsCollection::formImageElements() const
{
    ASSERT(isHTMLFormElement(ownerNode()));
    return toHTMLFormElement(ownerNode()).imageElements();
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

Node* HTMLFormControlsCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    const Vector<HTMLImageElement*>* imagesElements = ownerNode().hasTagName(fieldsetTag) ? nullptr : &formImageElements();
    if (HTMLElement* item = firstNamedItem(formControlElements(), imagesElements, idAttr, name))
        return item;

    return firstNamedItem(formControlElements(), imagesElements, nameAttr, name);
}

void HTMLFormControlsCollection::updateNameCache() const
{
    if (hasIdNameCache())
        return;

    HashSet<AtomicStringImpl*> foundInputElements;

    const Vector<FormAssociatedElement*>& elementsArray = formControlElements();

    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        FormAssociatedElement& associatedElement = *elementsArray[i];
        if (associatedElement.isEnumeratable()) {
            HTMLElement& element = associatedElement.asHTMLElement();
            const AtomicString& idAttrVal = element.getIdAttribute();
            const AtomicString& nameAttrVal = element.getNameAttribute();
            if (!idAttrVal.isEmpty()) {
                appendIdCache(idAttrVal, &element);
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                appendNameCache(nameAttrVal, &element);
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    if (isHTMLFormElement(ownerNode())) {
        const Vector<HTMLImageElement*>& imageElementsArray = formImageElements();
        for (unsigned i = 0; i < imageElementsArray.size(); ++i) {
            HTMLImageElement& element = *imageElementsArray[i];
            const AtomicString& idAttrVal = element.getIdAttribute();
            const AtomicString& nameAttrVal = element.getNameAttribute();
            if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl()))
                appendIdCache(idAttrVal, &element);
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl()))
                appendNameCache(nameAttrVal, &element);
        }
    }

    setHasIdNameCache();
}

void HTMLFormControlsCollection::invalidateCache() const
{
    HTMLCollection::invalidateCache();
    m_cachedElement = nullptr;
    m_cachedElementOffsetInArray = 0;
}

}
