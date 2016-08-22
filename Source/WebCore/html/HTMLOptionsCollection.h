/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
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

#pragma once

#include "CachedHTMLCollection.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

typedef int ExceptionCode;

class HTMLOptionsCollection final : public CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType> {
public:
    static Ref<HTMLOptionsCollection> create(HTMLSelectElement&, CollectionType);

    HTMLSelectElement& selectElement() { return downcast<HTMLSelectElement>(ownerNode()); }
    const HTMLSelectElement& selectElement() const { return downcast<HTMLSelectElement>(ownerNode()); }

    HTMLOptionElement* item(unsigned offset) const final;
    HTMLOptionElement* namedItem(const AtomicString& name) const final;

    WEBCORE_EXPORT void add(HTMLElement&, HTMLElement* beforeElement, ExceptionCode&);
    WEBCORE_EXPORT void add(HTMLElement&, int beforeIndex, ExceptionCode&);
    WEBCORE_EXPORT void remove(int index);
    void remove(HTMLOptionElement&);

    WEBCORE_EXPORT int selectedIndex() const;
    WEBCORE_EXPORT void setSelectedIndex(int);

    WEBCORE_EXPORT void setLength(unsigned, ExceptionCode&);

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
