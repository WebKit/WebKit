/*
 * Copyright (C) 2006, 2011, 2012 Apple Inc.
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
#include "HTMLOptionsCollection.h"

#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLOptionsCollection);

HTMLOptionsCollection::HTMLOptionsCollection(HTMLSelectElement& select)
    : CachedHTMLCollection<HTMLOptionsCollection, CollectionTypeTraits<SelectOptions>::traversalType>(select, SelectOptions)
{
}

Ref<HTMLOptionsCollection> HTMLOptionsCollection::create(HTMLSelectElement& select, CollectionType)
{
    return adoptRef(*new HTMLOptionsCollection(select));
}

ExceptionOr<void> HTMLOptionsCollection::add(const OptionOrOptGroupElement& element, const Optional<HTMLElementOrInt>& before)
{
    return selectElement().add(element, before);
}

void HTMLOptionsCollection::remove(int index)
{
    selectElement().remove(index);
}

int HTMLOptionsCollection::selectedIndex() const
{
    return selectElement().selectedIndex();
}

void HTMLOptionsCollection::setSelectedIndex(int index)
{
    selectElement().setSelectedIndex(index);
}

ExceptionOr<void> HTMLOptionsCollection::setLength(unsigned length)
{
    return selectElement().setLength(length);
}

} //namespace
