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

// FIXME: Generate from logical property groups.
// https://drafts.csswg.org/css-logical-1/#inset-properties
bool CSSProperty::isInsetProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyInset:
    case CSSPropertyLeft:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyBottom:

    case CSSPropertyInsetInline:
    case CSSPropertyInsetInlineStart:
    case CSSPropertyInsetInlineEnd:

    case CSSPropertyInsetBlock:
    case CSSPropertyInsetBlockStart:
    case CSSPropertyInsetBlockEnd:
        return true;
    default:
        return false;
    }
};

bool CSSProperty::isSizingProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyWidth:
    case CSSPropertyMinWidth:
    case CSSPropertyMaxWidth:

    case CSSPropertyHeight:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:

    case CSSPropertyBlockSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMaxBlockSize:

    case CSSPropertyInlineSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMaxInlineSize:
        return true;
    default:
        return false;
    }
}

bool CSSProperty::isMarginProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyMargin:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyMarginBottom:

    case CSSPropertyMarginBlock:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:

    case CSSPropertyMarginInline:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginInlineEnd:
        return true;

    default:
        return false;
    }
}


} // namespace WebCore
