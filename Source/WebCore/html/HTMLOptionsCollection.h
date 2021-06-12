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

class HTMLOptionsCollection final : public CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType> {
    WTF_MAKE_ISO_ALLOCATED(HTMLOptionsCollection);
public:
    using Base = CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType>;

    static Ref<HTMLOptionsCollection> create(HTMLSelectElement&, CollectionType);

    HTMLSelectElement& selectElement() { return downcast<HTMLSelectElement>(ownerNode()); }
    const HTMLSelectElement& selectElement() const { return downcast<HTMLSelectElement>(ownerNode()); }

    HTMLOptionElement* item(unsigned offset) const final;
    HTMLOptionElement* namedItem(const AtomString& name) const final;
    
    ExceptionOr<void> setItem(unsigned index, HTMLOptionElement*);
    
    using OptionOrOptGroupElement = Variant<RefPtr<HTMLOptionElement>, RefPtr<HTMLOptGroupElement>>;
    using HTMLElementOrInt = Variant<RefPtr<HTMLElement>, int>;
    WEBCORE_EXPORT ExceptionOr<void> add(const OptionOrOptGroupElement&, const std::optional<HTMLElementOrInt>& before);
    WEBCORE_EXPORT void remove(int index);

    WEBCORE_EXPORT int selectedIndex() const;
    WEBCORE_EXPORT void setSelectedIndex(int);

    WEBCORE_EXPORT ExceptionOr<void> setLength(unsigned);

    // For CachedHTMLCollection.
    bool elementMatches(Element&) const;

private:
    explicit HTMLOptionsCollection(HTMLSelectElement&);
};

inline HTMLOptionElement* HTMLOptionsCollection::item(unsigned offset) const
{
    return downcast<HTMLOptionElement>(Base::item(offset));
}

inline HTMLOptionElement* HTMLOptionsCollection::namedItem(const AtomString& name) const
{
    return downcast<HTMLOptionElement>(Base::namedItem(name));
}

inline ExceptionOr<void> HTMLOptionsCollection::setItem(unsigned index, HTMLOptionElement* optionElement)
{
    return selectElement().setItem(index, optionElement);
}

inline bool HTMLOptionsCollection::elementMatches(Element& element) const
{
    if (!element.hasTagName(HTMLNames::optionTag))
        return false;

    if (element.parentNode() == &selectElement())
        return true;

    ASSERT(element.parentNode());
    return element.parentNode()->hasTagName(HTMLNames::optgroupTag) && element.parentNode()->parentNode() == &selectElement();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_HTMLCOLLECTION(HTMLOptionsCollection, SelectOptions)
