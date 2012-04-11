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

#ifndef HTMLPropertiesCollection_h
#define HTMLPropertiesCollection_h

#if ENABLE(MICRODATA)

#include "DOMStringList.h"
#include "HTMLCollection.h"

namespace WebCore {

class DOMStringList;

class HTMLPropertiesCollection : public HTMLCollection {
public:
    static PassOwnPtr<HTMLPropertiesCollection> create(Node*);
    virtual ~HTMLPropertiesCollection();

    unsigned length() const OVERRIDE;

    virtual Node* item(unsigned) const OVERRIDE;

    PassRefPtr<DOMStringList> names() const;

    PassRefPtr<NodeList> namedItem(const String&) const;
    bool hasNamedItem(const AtomicString&) const;

private:
    HTMLPropertiesCollection(Node*);

    unsigned calcLength() const;
    void findProperties(Element* base) const;

    Node* findRefElements(Node* previous) const;

    Element* firstProperty() const;
    Element* itemAfter(Element* base, Element* previous) const;

    void updateNameCache() const;
    void updateRefElements() const;

    void invalidateCacheIfNeeded() const;

    mutable struct {
        uint64_t version;
        Element* current;
        unsigned position;
        unsigned length;
        bool hasLength;
        bool hasNameCache;
        NodeCacheMap propertyCache;
        Vector<Element*> itemRefElements;
        RefPtr<DOMStringList> propertyNames;
        unsigned itemRefElementPosition;
        bool hasItemRefElements;

        void clear()
        {
            version = 0;
            current = 0;
            position = 0;
            length = 0;
            hasLength = false;
            hasNameCache = false;
            propertyCache.clear();
            itemRefElements.clear();
            propertyNames.clear();
            itemRefElementPosition = 0;
            hasItemRefElements = false;
        }

        void setItemRefElements(const Vector<Element*>& elements)
        {
            itemRefElements = elements;
            hasItemRefElements = true;
        }

        const Vector<Element*>& getItemRefElements()
        {
            return itemRefElements;
        }

        void updateLength(unsigned len)
        {
            length = len;
            hasLength = true;
        }

        void updatePropertyCache(Element* element, const AtomicString& propertyName)
        {
            if (!propertyNames)
                propertyNames = DOMStringList::create();

            if (!propertyNames->contains(propertyName))
                propertyNames->append(propertyName);

            Vector<Element*>* propertyResults = propertyCache.get(propertyName.impl());
            if (!propertyResults || !propertyResults->contains(element))
                append(propertyCache, propertyName, element);
        }

        void updateCurrentItem(Element* element, unsigned pos, unsigned itemRefElementPos)
        {
            current = element;
            position = pos;
            itemRefElementPosition = itemRefElementPos;
        }

        void resetPosition()
        {
            current = 0;
            position = 0;
            itemRefElementPosition = 0;
        }

    } m_cache;

};

} // namespace WebCore

#endif // ENABLE(MICRODATA)

#endif // HTMLPropertiesCollection_h
