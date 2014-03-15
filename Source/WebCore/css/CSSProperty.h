/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
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

#ifndef CSSProperty_h
#define CSSProperty_h

#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "RenderStyleConstants.h"
#include "TextDirection.h"
#include "WritingMode.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

struct StylePropertyMetadata {
    StylePropertyMetadata(CSSPropertyID propertyID, bool isSetFromShorthand, int indexInShorthandsVector, bool important, bool implicit, bool inherited)
        : m_propertyID(propertyID)
        , m_isSetFromShorthand(isSetFromShorthand)
        , m_indexInShorthandsVector(indexInShorthandsVector)
        , m_important(important)
        , m_implicit(implicit)
        , m_inherited(inherited)
    {
    }

    CSSPropertyID shorthandID() const;

    uint16_t m_propertyID : 10;
    uint16_t m_isSetFromShorthand : 1;
    uint16_t m_indexInShorthandsVector : 2; // If this property was set as part of an ambiguous shorthand, gives the index in the shorthands vector.
    uint16_t m_important : 1;
    uint16_t m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.
    uint16_t m_inherited : 1;
};

class CSSProperty {
public:
    CSSProperty(CSSPropertyID propertyID, PassRefPtr<CSSValue> value, bool important = false, bool isSetFromShorthand = false, int indexInShorthandsVector = 0, bool implicit = false)
        : m_metadata(propertyID, isSetFromShorthand, indexInShorthandsVector, important, implicit, isInheritedProperty(propertyID))
        , m_value(value)
    {
    }

    // FIXME: Remove this.
    CSSProperty(StylePropertyMetadata metadata, CSSValue* value)
        : m_metadata(metadata)
        , m_value(value)
    {
    }

    CSSPropertyID id() const { return static_cast<CSSPropertyID>(m_metadata.m_propertyID); }
    bool isSetFromShorthand() const { return m_metadata.m_isSetFromShorthand; };
    CSSPropertyID shorthandID() const { return m_metadata.shorthandID(); };
    bool isImportant() const { return m_metadata.m_important; }

    CSSValue* value() const { return m_value.get(); }

    void wrapValueInCommaSeparatedList();

    static CSSPropertyID resolveDirectionAwareProperty(CSSPropertyID, TextDirection, WritingMode);
    static bool isInheritedProperty(CSSPropertyID);
    static bool isDirectionAwareProperty(CSSPropertyID);

    const StylePropertyMetadata& metadata() const { return m_metadata; }

private:
    StylePropertyMetadata m_metadata;
    RefPtr<CSSValue> m_value;
};

inline CSSPropertyID prefixingVariantForPropertyId(CSSPropertyID propId)
{
    CSSPropertyID propertyId = CSSPropertyInvalid;
    switch (propId) {
    case CSSPropertyTransitionDelay:
        propertyId = CSSPropertyWebkitTransitionDelay;
        break;
    case CSSPropertyTransitionDuration:
        propertyId = CSSPropertyWebkitTransitionDuration;
        break;
    case CSSPropertyTransitionProperty:
        propertyId = CSSPropertyWebkitTransitionProperty;
        break;
    case CSSPropertyTransitionTimingFunction:
        propertyId = CSSPropertyWebkitTransitionTimingFunction;
        break;
    case CSSPropertyTransition:
        propertyId = CSSPropertyWebkitTransition;
        break;
    case CSSPropertyWebkitTransitionDelay:
        propertyId = CSSPropertyTransitionDelay;
        break;
    case CSSPropertyWebkitTransitionDuration:
        propertyId = CSSPropertyTransitionDuration;
        break;
    case CSSPropertyWebkitTransitionProperty:
        propertyId = CSSPropertyTransitionProperty;
        break;
    case CSSPropertyWebkitTransitionTimingFunction:
        propertyId = CSSPropertyTransitionTimingFunction;
        break;
    case CSSPropertyWebkitTransition:
        propertyId = CSSPropertyTransition;
        break;
    default:
        propertyId = propId;
        break;
    }
    ASSERT(propertyId != CSSPropertyInvalid);
    return propertyId;
}

} // namespace WebCore

namespace WTF {
template <> struct VectorTraits<WebCore::CSSProperty> : VectorTraitsBase<false, WebCore::CSSProperty> {
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};
}

#endif // CSSProperty_h
