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

#include "CollectionCache.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

// Since the collections are to be "live", we have to do the
// calculation every time if anything has changed.

HTMLFormCollection::HTMLFormCollection(HTMLFormElement* form)
    : HTMLCollection(form, OtherCollection, /* retainBaseNode */ false)
{
}

PassRefPtr<HTMLFormCollection> HTMLFormCollection::create(HTMLFormElement* form)
{
    return adoptRef(new HTMLFormCollection(form));
}

HTMLFormCollection::~HTMLFormCollection()
{
}

unsigned HTMLFormCollection::calcLength() const
{
    ASSERT(base());
    return static_cast<HTMLFormElement*>(base())->length();
}

Node* HTMLFormCollection::item(unsigned index) const
{
    if (!base())
        return 0;

    resetCollectionInfo();

    if (info()->current && info()->position == index)
        return info()->current;

    if (info()->hasLength && info()->length <= index)
        return 0;

    if (!info()->current || info()->position > index) {
        info()->current = 0;
        info()->position = 0;
        info()->elementsArrayPosition = 0;
    }

    Vector<FormAssociatedElement*>& elementsArray = static_cast<HTMLFormElement*>(base())->m_associatedElements;
    unsigned currentIndex = info()->position;
    
    for (unsigned i = info()->elementsArrayPosition; i < elementsArray.size(); i++) {
        if (elementsArray[i]->isEnumeratable()) {
            HTMLElement* element = toHTMLElement(elementsArray[i]);
            if (index == currentIndex) {
                info()->position = index;
                info()->current = element;
                info()->elementsArrayPosition = i;
                return element;
            }

            currentIndex++;
        }
    }

    return 0;
}

Element* HTMLFormCollection::getNamedItem(const QualifiedName& attrName, const AtomicString& name) const
{
    if (!base())
        return 0;

    info()->position = 0;
    return getNamedFormItem(attrName, name, 0);
}

Element* HTMLFormCollection::getNamedFormItem(const QualifiedName& attrName, const String& name, int duplicateNumber) const
{
    HTMLFormElement* form = static_cast<HTMLFormElement*>(base());

    if (!form)
        return 0;

    bool foundInputElements = false;
    for (unsigned i = 0; i < form->m_associatedElements.size(); ++i) {
        FormAssociatedElement* associatedElement = form->m_associatedElements[i];
        HTMLElement* element = toHTMLElement(associatedElement);
        if (associatedElement->isEnumeratable() && element->getAttribute(attrName) == name) {
            foundInputElements = true;
            if (!duplicateNumber)
                return element;
            --duplicateNumber;
        }
    }

    if (!foundInputElements) {
        for (unsigned i = 0; i < form->m_imageElements.size(); ++i) {
            HTMLImageElement* element = form->m_imageElements[i];
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
    return item(info()->position + 1);
}

Node* HTMLFormCollection::namedItem(const AtomicString& name) const
{
    if (!base())
        return 0;

    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    info()->current = getNamedItem(idAttr, name);
    if (info()->current)
        return info()->current;
    info()->current = getNamedItem(nameAttr, name);
    return info()->current;
}

void HTMLFormCollection::updateNameCache() const
{
    if (!base())
        return;

    if (info()->hasNameCache)
        return;

    HashSet<AtomicStringImpl*> foundInputElements;

    HTMLFormElement* f = static_cast<HTMLFormElement*>(base());

    for (unsigned i = 0; i < f->m_associatedElements.size(); ++i) {
        FormAssociatedElement* associatedElement = f->m_associatedElements[i];
        if (associatedElement->isEnumeratable()) {
            HTMLElement* element = toHTMLElement(associatedElement);
            const AtomicString& idAttrVal = element->getIdAttribute();
            const AtomicString& nameAttrVal = element->getAttribute(nameAttr);
            if (!idAttrVal.isEmpty()) {
                append(info()->idCache, idAttrVal, element);
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                append(info()->nameCache, nameAttrVal, element);
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    for (unsigned i = 0; i < f->m_imageElements.size(); ++i) {
        HTMLImageElement* element = f->m_imageElements[i];
        const AtomicString& idAttrVal = element->getIdAttribute();
        const AtomicString& nameAttrVal = element->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl()))
            append(info()->idCache, idAttrVal, element);
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl()))
            append(info()->nameCache, nameAttrVal, element);
    }

    info()->hasNameCache = true;
}

}
