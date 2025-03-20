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

#pragma once

#include "CSSPropertyNames.h"
#include "CSSValue.h"
#include "WritingMode.h"
#include <wtf/BitSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CSSValueList;
class Settings;

enum class IsImportant : bool { No, Yes };

struct StylePropertyMetadata {
    StylePropertyMetadata(CSSPropertyID propertyID, bool isSetFromShorthand, int indexInShorthandsVector, IsImportant important, bool implicit)
        : m_propertyID(propertyID)
        , m_isSetFromShorthand(isSetFromShorthand)
        , m_indexInShorthandsVector(indexInShorthandsVector)
        , m_important(important == IsImportant::Yes)
        , m_implicit(implicit)
    {
        ASSERT(propertyID != CSSPropertyInvalid);
        ASSERT_WITH_MESSAGE(propertyID < firstShorthandProperty, "unexpected property: %d", propertyID);
    }

    CSSPropertyID shorthandID() const;
    
    friend bool operator==(const StylePropertyMetadata&, const StylePropertyMetadata&) = default;

    unsigned m_propertyID : 10;
    unsigned m_isSetFromShorthand : 1;
    unsigned m_indexInShorthandsVector : 2; // If this property was set as part of an ambiguous shorthand, gives the index in the shorthands vector.
    unsigned m_important : 1;
    unsigned m_implicit : 1; // Whether or not the property was set implicitly as the result of a shorthand.
    // 1 bit available
};

class CSSProperty {
public:
    CSSProperty(CSSPropertyID propertyID, Ref<CSSValue>&& value, IsImportant important = IsImportant::No, bool isSetFromShorthand = false, int indexInShorthandsVector = 0, bool implicit = false)
        : m_metadata(propertyID, isSetFromShorthand, indexInShorthandsVector, important, implicit)
        , m_value(WTFMove(value))
    {
    }

    CSSPropertyID id() const { return static_cast<CSSPropertyID>(m_metadata.m_propertyID); }
    bool isSetFromShorthand() const { return m_metadata.m_isSetFromShorthand; };
    CSSPropertyID shorthandID() const { return m_metadata.shorthandID(); };
    bool isImportant() const { return m_metadata.m_important; }

    CSSValue* value() const { return m_value.ptr(); }
    Ref<CSSValue> protectedValue() const { return m_value; }

    static CSSPropertyID resolveDirectionAwareProperty(CSSPropertyID, WritingMode);
    static CSSPropertyID unresolvePhysicalProperty(CSSPropertyID, WritingMode);
    static bool isInheritedProperty(CSSPropertyID);
    static Vector<String> aliasesForProperty(CSSPropertyID);
    static bool isDirectionAwareProperty(CSSPropertyID);
    static bool isInLogicalPropertyGroup(CSSPropertyID);
    static bool areInSameLogicalPropertyGroupWithDifferentMappingLogic(CSSPropertyID, CSSPropertyID);
    static bool isDescriptorOnly(CSSPropertyID);
    static UChar listValuedPropertySeparator(CSSPropertyID);
    static bool isListValuedProperty(CSSPropertyID propertyID) { return !!listValuedPropertySeparator(propertyID); }
    static bool allowsNumberOrIntegerInput(CSSPropertyID);

    static bool animationUsesNonAdditiveOrCumulativeInterpolation(CSSPropertyID);
    static bool animationUsesNonNormalizedDiscreteInterpolation(CSSPropertyID);

    static bool animationIsAccelerated(CSSPropertyID, const Settings&);
    static std::span<const CSSPropertyID> allAcceleratedAnimationProperties(const Settings&);

    // Logical Property Groups.
    // NOTE: These return true if the CSSPropertyID is member of the named logical
    // property group or is the shorthand of a member of the logical property group.

    static bool isBorderColorProperty(CSSPropertyID);
    static bool isBorderRadiusProperty(CSSPropertyID);
    static bool isBorderStyleProperty(CSSPropertyID);
    static bool isBorderWidthProperty(CSSPropertyID);
    static bool isContainIntrinsicSizeProperty(CSSPropertyID);
    static bool isCornerShapeProperty(CSSPropertyID);
    static bool isInsetProperty(CSSPropertyID);
    static bool isMarginProperty(CSSPropertyID);
    static bool isMaxSizeProperty(CSSPropertyID);
    static bool isMinSizeProperty(CSSPropertyID);
    static bool isOverflowProperty(CSSPropertyID);
    static bool isOverscrollBehaviorProperty(CSSPropertyID);
    static bool isPaddingProperty(CSSPropertyID);
    static bool isScrollMarginProperty(CSSPropertyID);
    static bool isScrollPaddingProperty(CSSPropertyID);
    static bool isSizeProperty(CSSPropertyID);

    // Check if a property is a sizing property, as defined in:
    // https://drafts.csswg.org/css-sizing-3/#sizing-property
    static bool isSizingProperty(CSSPropertyID);

    static bool disablesNativeAppearance(CSSPropertyID);

    const StylePropertyMetadata& metadata() const { return m_metadata; }
    static bool isColorProperty(CSSPropertyID propertyId)
    {
        return colorProperties.get(propertyId);
    }

    static const WEBCORE_EXPORT WTF::BitSet<cssPropertyIDEnumValueCount> colorProperties;
    static const WEBCORE_EXPORT WTF::BitSet<cssPropertyIDEnumValueCount> physicalProperties;

    bool operator==(const CSSProperty& other) const
    {
        if (!(m_metadata == other.m_metadata))
            return false;
        return m_value->equals(other.m_value);
    }

private:
    StylePropertyMetadata m_metadata;
    Ref<CSSValue> m_value;
};

typedef Vector<CSSProperty, 256> ParsedPropertyVector;

} // namespace WebCore

namespace WTF {
template <> struct VectorTraits<WebCore::CSSProperty> : VectorTraitsBase<false, WebCore::CSSProperty> {
    static const bool canInitializeWithMemset = true;
    static const bool canMoveWithMemcpy = true;
};
}
