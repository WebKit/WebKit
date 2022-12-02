/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2021 Apple Inc.
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
 */

#include "config.h"
#include "CSSProperty.h"

#include "CSSValueList.h"
#include "StylePropertyShorthand.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

struct SameSizeAsCSSProperty {
    uint32_t bitfields;
    void* value;
};

static_assert(sizeof(CSSProperty) == sizeof(SameSizeAsCSSProperty), "CSSProperty should stay small");

CSSPropertyID StylePropertyMetadata::shorthandID() const
{
    if (!m_isSetFromShorthand)
        return CSSPropertyInvalid;

    auto shorthands = matchingShorthandsForLonghand(static_cast<CSSPropertyID>(m_propertyID));
    ASSERT(shorthands.size() && m_indexInShorthandsVector >= 0 && m_indexInShorthandsVector < shorthands.size());
    return shorthands[m_indexInShorthandsVector].id();
}

void CSSProperty::wrapValueInCommaSeparatedList()
{
    auto list = CSSValueList::createCommaSeparated();
    list.get().append(m_value.releaseNonNull());
    m_value = WTFMove(list);
}

Ref<CSSValueList> CSSProperty::createListForProperty(CSSPropertyID propertyID)
{
    switch (listValuedPropertySeparator(propertyID)) {
    case ' ':
        return CSSValueList::createSpaceSeparated();
    case ',':
        return CSSValueList::createCommaSeparated();
    case '/':
        return CSSValueList::createSlashSeparated();
    default:
        break;
    }
    ASSERT_NOT_REACHED();
    return CSSValueList::createCommaSeparated();
}

} // namespace WebCore
