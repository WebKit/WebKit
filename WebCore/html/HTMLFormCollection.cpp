/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

inline CollectionCache* HTMLFormCollection::formCollectionInfo(HTMLFormElement* form)
{
    if (!form->collectionInfo)
        form->collectionInfo = new CollectionCache;
    return form->collectionInfo;
}

HTMLFormCollection::HTMLFormCollection(PassRefPtr<HTMLFormElement> form)
    : HTMLCollection(form.get(), OtherCollection, formCollectionInfo(form.get()))
{
}

PassRefPtr<HTMLFormCollection> HTMLFormCollection::create(PassRefPtr<HTMLFormElement> form)
{
    return adoptRef(new HTMLFormCollection(form));
}

HTMLFormCollection::~HTMLFormCollection()
{
}

unsigned HTMLFormCollection::calcLength() const
{
    return static_cast<HTMLFormElement*>(base())->length();
}

Node* HTMLFormCollection::item(unsigned index) const
{
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

    Vector<HTMLFormControlElement*>& l = static_cast<HTMLFormElement*>(base())->formElements;
    unsigned currentIndex = info()->position;
    
    for (unsigned i = info()->elementsArrayPosition; i < l.size(); i++) {
        if (l[i]->isEnumeratable() ) {
            if (index == currentIndex) {
                info()->position = index;
                info()->current = l[i];
                info()->elementsArrayPosition = i;
                return l[i];
            }

            currentIndex++;
        }
    }

    return 0;
}

Element* HTMLFormCollection::getNamedItem(const QualifiedName& attrName, const AtomicString& name) const
{
    info()->position = 0;
    return getNamedFormItem(attrName, name, 0);
}

Element* HTMLFormCollection::getNamedFormItem(const QualifiedName& attrName, const String& name, int duplicateNumber) const
{
    HTMLFormElement* form = static_cast<HTMLFormElement*>(base());

    bool foundInputElements = false;
    for (unsigned i = 0; i < form->formElements.size(); ++i) {
        HTMLFormControlElement* e = form->formElements[i];
        const QualifiedName& attributeName = (attrName == idAttr) ? e->idAttributeName() : attrName;
        if (e->isEnumeratable() && e->getAttribute(attributeName) == name) {
            foundInputElements = true;
            if (!duplicateNumber)
                return e;
            --duplicateNumber;
        }
    }

    if (!foundInputElements) {
        for (unsigned i = 0; i < form->imgElements.size(); ++i) {
            HTMLImageElement* e = form->imgElements[i];
            const QualifiedName& attributeName = (attrName == idAttr) ? e->idAttributeName() : attrName;
            if (e->getAttribute(attributeName) == name) {
                if (!duplicateNumber)
                    return e;
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

Element* HTMLFormCollection::nextNamedItemInternal(const String &name) const
{
    Element* retval = getNamedFormItem(m_idsDone ? nameAttr : idAttr, name, ++info()->position);
    if (retval)
        return retval;
    if (m_idsDone) // we're done
        return 0;
    // After doing id, do name
    m_idsDone = true;
    return getNamedItem(nameAttr, name);
}

Node* HTMLFormCollection::namedItem(const AtomicString& name) const
{
    // http://msdn.microsoft.com/workshop/author/dhtml/reference/methods/nameditem.asp
    // This method first searches for an object with a matching id
    // attribute. If a match is not found, the method then searches for an
    // object with a matching name attribute, but only on those elements
    // that are allowed a name attribute.
    resetCollectionInfo();
    m_idsDone = false;
    info()->current = getNamedItem(idAttr, name);
    if (info()->current)
        return info()->current;
    m_idsDone = true;
    info()->current = getNamedItem(nameAttr, name);
    return info()->current;
}

Node* HTMLFormCollection::nextNamedItem(const AtomicString& name) const
{
    // The nextNamedItemInternal function can return the same item twice if it has
    // both an id and name that are equal to the name parameter. So this function
    // checks if we are on the nameAttr half of the iteration and skips over any
    // that also have the same idAttributeName.
    Element* impl = nextNamedItemInternal(name);
    if (m_idsDone)
        while (impl && impl->getAttribute(impl->idAttributeName()) == name)
            impl = nextNamedItemInternal(name);
    return impl;
}

void HTMLFormCollection::updateNameCache() const
{
    if (info()->hasNameCache)
        return;

    HashSet<AtomicStringImpl*> foundInputElements;

    HTMLFormElement* f = static_cast<HTMLFormElement*>(base());

    for (unsigned i = 0; i < f->formElements.size(); ++i) {
        HTMLFormControlElement* e = f->formElements[i];
        if (e->isEnumeratable()) {
            const AtomicString& idAttrVal = e->getAttribute(e->idAttributeName());
            const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
            if (!idAttrVal.isEmpty()) {
                // add to id cache
                Vector<Element*>* idVector = info()->idCache.get(idAttrVal.impl());
                if (!idVector) {
                    idVector = new Vector<Element*>;
                    info()->idCache.add(idAttrVal.impl(), idVector);
                }
                idVector->append(e);
                foundInputElements.add(idAttrVal.impl());
            }
            if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal) {
                // add to name cache
                Vector<Element*>* nameVector = info()->nameCache.get(nameAttrVal.impl());
                if (!nameVector) {
                    nameVector = new Vector<Element*>;
                    info()->nameCache.add(nameAttrVal.impl(), nameVector);
                }
                nameVector->append(e);
                foundInputElements.add(nameAttrVal.impl());
            }
        }
    }

    for (unsigned i = 0; i < f->imgElements.size(); ++i) {
        HTMLImageElement* e = f->imgElements[i];
        const AtomicString& idAttrVal = e->getAttribute(e->idAttributeName());
        const AtomicString& nameAttrVal = e->getAttribute(nameAttr);
        if (!idAttrVal.isEmpty() && !foundInputElements.contains(idAttrVal.impl())) {
            // add to id cache
            Vector<Element*>* idVector = info()->idCache.get(idAttrVal.impl());
            if (!idVector) {
                idVector = new Vector<Element*>;
                info()->idCache.add(idAttrVal.impl(), idVector);
            }
            idVector->append(e);
        }
        if (!nameAttrVal.isEmpty() && idAttrVal != nameAttrVal && !foundInputElements.contains(nameAttrVal.impl())) {
            // add to name cache
            Vector<Element*>* nameVector = info()->nameCache.get(nameAttrVal.impl());
            if (!nameVector) {
                nameVector = new Vector<Element*>;
                info()->nameCache.add(nameAttrVal.impl(), nameVector);
            }
            nameVector->append(e);
        }
    }

    info()->hasNameCache = true;
}

}
