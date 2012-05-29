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
#include "HTMLFormCollection.h"

#include "HTMLFieldSetElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormCollection::HTMLFormCollection(HTMLElement* base)
    : HTMLCollection(base, FormControls)
    , currentPos(0)
{
}

PassOwnPtr<HTMLFormCollection> HTMLFormCollection::create(HTMLElement* base)
{
    return adoptPtr(new HTMLFormCollection(base));
}

HTMLFormCollection::~HTMLFormCollection()
{
}

const Vector<FormAssociatedElement*>& HTMLFormCollection::formControlElements() const
{
    ASSERT(base());
    ASSERT(base()->hasTagName(formTag) || base()->hasTagName(fieldsetTag));
    if (base()->hasTagName(formTag))
        return static_cast<HTMLFormElement*>(base())->associatedElements();
    return static_cast<HTMLFieldSetElement*>(base())->associatedElements();
}

const Vector<HTMLImageElement*>& HTMLFormCollection::formImageElements() const
{
    ASSERT(base());
    ASSERT(base()->hasTagName(formTag));
    return static_cast<HTMLFormElement*>(base())->imageElements();
}

unsigned HTMLFormCollection::numberOfFormControlElements() const
{
    ASSERT(base());
    ASSERT(base()->hasTagName(formTag) || base()->hasTagName(fieldsetTag));
    if (base()->hasTagName(formTag))
        return static_cast<HTMLFormElement*>(base())->length();
    return static_cast<HTMLFieldSetElement*>(base())->length();
}

unsigned HTMLFormCollection::calcLength() const
{
    return numberOfFormControlElements();
}

Node* HTMLFormCollection::item(unsigned index) const
{
    invalidateCacheIfNeeded();

    if (m_cache.current && m_cache.position == index)
        return m_cache.current;

    if (m_cache.hasLength && m_cache.length <= index)
        return 0;

    if (!m_cache.current || m_cache.position > index) {
        m_cache.current = 0;
        m_cache.position = 0;
        m_cache.elementsArrayPosition = 0;
    }

    const Vector<FormAssociatedElement*>& elementsArray = formControlElements();
    unsigned currentIndex = m_cache.position;
    
    for (unsigned i = m_cache.elementsArrayPosition; i < elementsArray.size(); i++) {
        if (elementsArray[i]->isEnumeratable()) {
            HTMLElement* element = toHTMLElement(elementsArray[i]);
            if (index == currentIndex) {
                m_cache.position = index;
                m_cache.current = element;
                m_cache.elementsArrayPosition = i;
                return element;
            }

            currentIndex++;
        }
    }

    return 0;
}

Element* HTMLFormCollection::getNamedItem(const QualifiedName& attrName, const AtomicString& name) const
{
    m_cache.position = 0;
    return getNamedFormItem(attrName, name, 0);
}

Element* HTMLFormCollection::getNamedFormItem(const QualifiedName& attrName, const String& name, int duplicateNumber) const
{
    const Vector<FormAssociatedElement*>& elementsArray = formControlElements();

    bool foundInputElements = false;
    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        FormAssociatedElement* associatedElement = elementsArray[i];
        HTMLElement* element = toHTMLElement(associatedElement);
        if (associatedElement->isEnumeratable() && element->getAttribute(attrName) == name) {
            foundInputElements = true;
            if (!duplicateNumber)
                return element;
            --duplicateNumber;
        }
    }

    if (base()->hasTagName(fieldsetTag))
        return 0;

    const Vector<HTMLImageElement*>& imageElementsArray = formImageElements();
    if (!foundInputElements) {
        for (unsigned i = 0; i < imageElementsArray.size(); ++i) {
            HTMLImageElement* element = imageElementsArray[i];
            if (element->getAttribute(attrName) == name) {
                if (!duplicateNumber)
                    return element;
                --duplicateNumber;
            }
        }
    }

    return 0;
}

Node* HTMLFormCollection::nextItem() const
{
    return item(m_cache.position + 1);
}

Node* HTMLFormCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    invalidateCacheIfNeeded();
    m_cache.current = getNamedItem(idAttr, name);
    if (m_cache.current)
        return m_cache.current;
    m_cache.current = getNamedItem(nameAttr, name);
    return m_cache.current;
}

void HTMLFormCollection::updateNameCache() const
{
    if (m_cache.hasNameCache)
        return;

    HashSet<AtomicStringImpl*> foundInputElements;

    const Vector<FormAssociatedElement*>& elementsArray = formControlElements();

    for (unsigned i = 0; i < elementsArray.size(); ++i) {
        FormAssociatedElement* associatedElement = elementsArray[i];
        if (associatedElement->isEnumeratable()) {
            HTMLElement* element = toHTMLElement(associatedElement);
            const AtomicString& idAttrVal = element->getIdAttribute();
            const AtomicString& nameAttrVal = element->getNameAttribute();
            if (!idAttrVal.isEmpty()) {
                append(m_cache.idCache, idAttrVal, element);
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                append(m_cache.nameCache, nameAttrVal, element);
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    if (base()->hasTagName(formTag)) {
        const Vector<HTMLImageElement*>& imageElementsArray = formImageElements();
        for (unsigned i = 0; i < imageElementsArray.size(); ++i) {
            HTMLImageElement* element = imageElementsArray[i];
            const AtomicString& idAttrVal = element->getIdAttribute();
            const AtomicString& nameAttrVal = element->getNameAttribute();
            if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl()))
                append(m_cache.idCache, idAttrVal, element);
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl()))
                append(m_cache.nameCache, nameAttrVal, element);
        }
    }

    m_cache.hasNameCache = true;
}

}
