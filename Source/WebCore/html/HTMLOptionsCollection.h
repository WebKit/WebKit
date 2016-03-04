/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLOptionsCollection_h
#define HTMLOptionsCollection_h

#include "CachedHTMLCollection.h"
#include "HTMLSelectElement.h"

namespace WebCore {

class HTMLOptionElement;

typedef int ExceptionCode;

class HTMLOptionsCollection final : public CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType> {
public:
    static Ref<HTMLOptionsCollection> create(HTMLSelectElement&, CollectionType);

    HTMLSelectElement& selectElement() { return downcast<HTMLSelectElement>(ownerNode()); }
    const HTMLSelectElement& selectElement() const { return downcast<HTMLSelectElement>(ownerNode()); }

    HTMLOptionElement* item(unsigned offset) const override;
    HTMLOptionElement* namedItem(const AtomicString& name) const override;

    void add(HTMLElement*, HTMLElement* beforeElement, ExceptionCode&);
    void add(HTMLElement*, int beforeIndex, ExceptionCode&);
    void remove(int index);
    void remove(HTMLOptionElement*);

    int selectedIndex() const;
    void setSelectedIndex(int);

    void setLength(unsigned, ExceptionCode&);

    // For CachedHTMLCollection.
    bool elementMatches(Element&) const;

private:
    explicit HTMLOptionsCollection(HTMLSelectElement&);
};

inline HTMLOptionElement* HTMLOptionsCollection::item(unsigned offset) const
{
    return downcast<HTMLOptionElement>(CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType>::item(offset));
}

inline HTMLOptionElement* HTMLOptionsCollection::namedItem(const AtomicString& name) const
{
    return downcast<HTMLOptionElement>(CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType>::namedItem(name));
}

inline bool HTMLOptionsCollection::elementMatches(Element& element) const
{
    return element.hasTagName(HTMLNames::optionTag);
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(HTMLOptionsCollection, SelectOptions)

#endif // HTMLOptionsCollection_h
