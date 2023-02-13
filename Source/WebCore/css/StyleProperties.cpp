/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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

#include "config.h"
#include "StyleProperties.h"

#include "CSSCustomPropertyValue.h"
#include "CSSParser.h"
#include "CSSPendingSubstitutionValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParserHelpers.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "Color.h"
#include "PropertySetCSSStyleDeclaration.h"
#include "ShorthandSerializer.h"
#include "StylePropertiesInlines.h"
#include "StylePropertyShorthand.h"
#include <bitset>
#include <wtf/text/StringBuilder.h>

#ifndef NDEBUG
#include <stdio.h>
#include <wtf/text/CString.h>
#endif

namespace WebCore {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StyleProperties);

constexpr unsigned maxShorthandsForLonghand = 4; // FIXME: Generate this from CSSProperties.json and use it for StylePropertyShorthandVector too.

Ref<ImmutableStyleProperties> StyleProperties::immutableCopyIfNeeded() const
{
    if (is<ImmutableStyleProperties>(*this))
        return downcast<ImmutableStyleProperties>(const_cast<StyleProperties&>(*this));
    const MutableStyleProperties& mutableThis = downcast<MutableStyleProperties>(*this);
    return ImmutableStyleProperties::create(mutableThis.m_propertyVector.data(), mutableThis.m_propertyVector.size(), cssParserMode());
}

String serializeLonghandValue(CSSPropertyID property, const CSSValue& value)
{
    switch (property) {
    case CSSPropertyFillOpacity:
    case CSSPropertyFloodOpacity:
    case CSSPropertyOpacity:
    case CSSPropertyStopOpacity:
    case CSSPropertyStrokeOpacity:
        // FIXME: Handle this when creating the CSSValue for opacity, to be consistent with other CSS value serialization quirks.
        // Opacity percentage values serialize as a fraction in the range 0-1, not "%".
        if (is<CSSPrimitiveValue>(value) && downcast<CSSPrimitiveValue>(value).isPercentage())
            return makeString(downcast<CSSPrimitiveValue>(value).doubleValue() / 100);
        break;
    default:
        break;
    }
    // Longhands set by mask and background shorthands can have comma-separated lists with implicit initial values in them.
    // We need to serialize those lists with the actual values, not as "initial".
    // Doing this for all CSSValueList with comma separators is better than checking the property is one of those longhands.
    // Serializing this way is harmless for other properties; those won't have any implicit initial values.
    if (auto* list = dynamicDowncast<CSSValueList>(value); list && list->separator() == CSSValueList::CommaSeparator) {
        StringBuilder result;
        auto separator = ""_s;
        for (auto& individualValue : *list)
            result.append(std::exchange(separator, ", "_s), serializeLonghandValue(property, individualValue));
        return result.toString();
    }
    return value.isImplicitInitialValue() ? initialValueTextForLonghand(property) : value.cssText();
}

inline String StyleProperties::serializeLonghandValue(CSSPropertyID propertyID) const
{
    return WebCore::serializeLonghandValue(propertyID, getPropertyCSSValue(propertyID).get());
}

inline String StyleProperties::serializeShorthandValue(CSSPropertyID propertyID) const
{
    return WebCore::serializeShorthandValue(*this, propertyID);
}

String StyleProperties::getPropertyValue(CSSPropertyID propertyID) const
{
    return isLonghand(propertyID) ? serializeLonghandValue(propertyID) : serializeShorthandValue(propertyID);
}

std::optional<Color> StyleProperties::propertyAsColor(CSSPropertyID property) const
{
    auto value = getPropertyCSSValue(property);
    if (!value)
        return std::nullopt;
    return value->isColor() ? value->color()
        : CSSParser::parseColorWithoutContext(WebCore::serializeLonghandValue(property, *value));
}

std::optional<CSSValueID> StyleProperties::propertyAsValueID(CSSPropertyID property) const
{
    return longhandValueID(property, getPropertyCSSValue(property).get());
}

String StyleProperties::getCustomPropertyValue(const String& propertyName) const
{
    RefPtr<CSSValue> value = getCustomPropertyCSSValue(propertyName);
    if (value)
        return value->cssText();
    return String();
}

RefPtr<CSSValue> StyleProperties::getPropertyCSSValue(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return nullptr;
    auto property = propertyAt(foundPropertyIndex);
    auto value = property.value();
    // System fonts are represented as CSSPrimitiveValue for various font subproperties, but these must serialize as the empty string.
    // It might be better to implement this as a special CSSValue type instead of turning them into null here.
    if (property.shorthandID() == CSSPropertyFont && CSSPropertyParserHelpers::isSystemFontShorthand(valueID(value)))
        return nullptr;
    return value;
}

RefPtr<CSSValue> StyleProperties::getCustomPropertyCSSValue(const String& propertyName) const
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex == -1)
        return nullptr;
    return propertyAt(foundPropertyIndex).value();
}

bool StyleProperties::propertyIsImportant(CSSPropertyID propertyID) const
{
    if (isLonghand(propertyID)) {
        int foundPropertyIndex = findPropertyIndex(propertyID);
        if (foundPropertyIndex != -1)
            return propertyAt(foundPropertyIndex).isImportant();
        return false;
    }

    ASSERT(shorthandForProperty(propertyID).length());
    for (auto longhand : shorthandForProperty(propertyID)) {
        if (!propertyIsImportant(longhand))
            return false;
    }
    return true;
}

bool StyleProperties::customPropertyIsImportant(const String& propertyName) const
{
    int foundPropertyIndex = findCustomPropertyIndex(propertyName);
    if (foundPropertyIndex != -1)
        return propertyAt(foundPropertyIndex).isImportant();
    return false;
}

String StyleProperties::getPropertyShorthand(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return String();
    return nameString(propertyAt(foundPropertyIndex).shorthandID());
}

bool StyleProperties::isPropertyImplicit(CSSPropertyID propertyID) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).isImplicit();
}

String StyleProperties::asText() const
{
    return asTextInternal().toString();
}

AtomString StyleProperties::asTextAtom() const
{
    return asTextInternal().toAtomString();
}

static constexpr bool canUseShorthandForLonghand(CSSPropertyID shorthandID, CSSPropertyID longhandID)
{
    ASSERT(isShorthand(shorthandID));
    ASSERT(isLonghand(longhandID));
    switch (shorthandID) {
    // We are not yet using the CSSPropertyFont shorthand here because our editing code is currently incompatible.
    case CSSPropertyFont:
        return false;

    // Avoid legacy shorthands according to https://www.w3.org/TR/css-cascade-5/#legacy-shorthand
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyWebkitBorderRadius:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitPerspective:
    case CSSPropertyWebkitTextDecoration:
    case CSSPropertyWebkitTextOrientation:
        return false;

    // No other browser currently supports text-decoration-skip, so it's currently more web
    // compatible to avoid collapsing text-decoration-skip-ink, its only longhand.
    case CSSPropertyTextDecorationSkip:
        return false;

    // FIXME: -webkit-mask is a legacy shorthand but it's used to serialize -webkit-mask-clip,
    // which should be a legacy shorthand of mask-clip, but it's implemented as a longhand.
    case CSSPropertyWebkitMask:
        return longhandID == CSSPropertyWebkitMaskClip;

    // FIXME: more mask nonsense.
    case CSSPropertyMask:
        return longhandID != CSSPropertyMaskComposite && longhandID != CSSPropertyMaskMode && longhandID != CSSPropertyMaskSize;

    // FIXME: If font-variant-ligatures is none, this depends on the value of the longhand.
    case CSSPropertyFontVariant:
    // FIXME: These shorthands are avoided for unknown legacy reasons, probably shouldn't be avoided.
    case CSSPropertyBorderBlockEnd:
    case CSSPropertyBorderBlockStart:
    case CSSPropertyBorderBottom:
    case CSSPropertyBorderInlineEnd:
    case CSSPropertyBorderInlineStart:
    case CSSPropertyBorderLeft:
    case CSSPropertyBorderRight:
    case CSSPropertyBorderTop:
    case CSSPropertyColumnRule:
    case CSSPropertyColumns:
    case CSSPropertyContainer:
    case CSSPropertyFontSynthesis:
    case CSSPropertyGap:
    case CSSPropertyGridArea:
    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
    case CSSPropertyMaskPosition:
    case CSSPropertyOffset:
    case CSSPropertyPlaceContent:
    case CSSPropertyPlaceItems:
    case CSSPropertyPlaceSelf:
    case CSSPropertyTextEmphasis:
    case CSSPropertyWebkitTextStroke:
        return false;

    default:
        return true;
    }
}

StringBuilder StyleProperties::asTextInternal() const
{
    StringBuilder result;

    constexpr unsigned shorthandPropertyCount = lastShorthandProperty - firstShorthandProperty + 1;
    std::bitset<shorthandPropertyCount> shorthandPropertyUsed;
    std::bitset<shorthandPropertyCount> shorthandPropertyAppeared;

    unsigned numDecls = 0;
    for (auto property : *this) {
        auto propertyID = property.id();
        ASSERT(isLonghand(propertyID) || propertyID == CSSPropertyCustom);
        Vector<CSSPropertyID, maxShorthandsForLonghand> shorthands;

        if (is<CSSPendingSubstitutionValue>(property.value())) {
            auto& substitutionValue = downcast<CSSPendingSubstitutionValue>(*property.value());
            shorthands.append(substitutionValue.shorthandPropertyId());
        } else {
            for (auto& shorthand : matchingShorthandsForLonghand(propertyID)) {
                if (canUseShorthandForLonghand(shorthand.id(), propertyID))
                    shorthands.append(shorthand.id());
            }
        }

        String value;
        bool alreadyUsedShorthand = false;
        for (auto& shorthandPropertyID : shorthands) {
            ASSERT(isShorthand(shorthandPropertyID));
            unsigned shorthandPropertyIndex = shorthandPropertyID - firstShorthandProperty;

            ASSERT(shorthandPropertyIndex < shorthandPropertyUsed.size());
            if (shorthandPropertyUsed[shorthandPropertyIndex]) {
                alreadyUsedShorthand = true;
                break;
            }
            if (shorthandPropertyAppeared[shorthandPropertyIndex])
                continue;
            shorthandPropertyAppeared.set(shorthandPropertyIndex);

            value = serializeShorthandValue(shorthandPropertyID);
            if (!value.isNull()) {
                propertyID = shorthandPropertyID;
                shorthandPropertyUsed.set(shorthandPropertyIndex);
                break;
            }
        }
        if (alreadyUsedShorthand)
            continue;

        if (value.isNull())
            value = WebCore::serializeLonghandValue(propertyID, *property.value());

        if (numDecls++)
            result.append(' ');

        if (propertyID == CSSPropertyCustom)
            result.append(downcast<CSSCustomPropertyValue>(*property.value()).name());
        else
            result.append(nameLiteral(propertyID));

        result.append(": ", value, property.isImportant() ? " !important" : "", ';');
    }

    ASSERT(!numDecls ^ !result.isEmpty());
    return result;
}

bool StyleProperties::hasCSSOMWrapper() const
{
    return is<MutableStyleProperties>(*this) && downcast<MutableStyleProperties>(*this).m_cssomWrapper;
}

bool StyleProperties::traverseSubresources(const Function<bool(const CachedResource&)>& handler) const
{
    for (auto property : *this) {
        if (property.value()->traverseSubresources(handler))
            return true;
    }
    return false;
}

bool StyleProperties::propertyMatches(CSSPropertyID propertyID, const CSSValue* propertyValue) const
{
    int foundPropertyIndex = findPropertyIndex(propertyID);
    if (foundPropertyIndex == -1)
        return false;
    return propertyAt(foundPropertyIndex).value()->equals(*propertyValue);
}

Ref<MutableStyleProperties> StyleProperties::mutableCopy() const
{
    return adoptRef(*new MutableStyleProperties(*this));
}

Ref<MutableStyleProperties> StyleProperties::copyProperties(Span<const CSSPropertyID> properties) const
{
    Vector<CSSProperty> vector;
    vector.reserveInitialCapacity(properties.size());
    for (auto property : properties) {
        if (auto value = getPropertyCSSValue(property))
            vector.uncheckedAppend(CSSProperty(property, WTFMove(value), false));
    }
    vector.shrinkToFit();
    return MutableStyleProperties::create(WTFMove(vector));
}

PropertySetCSSStyleDeclaration* MutableStyleProperties::cssStyleDeclaration()
{
    return m_cssomWrapper.get();
}

CSSStyleDeclaration& MutableStyleProperties::ensureCSSStyleDeclaration()
{
    if (m_cssomWrapper) {
        ASSERT(!static_cast<CSSStyleDeclaration*>(m_cssomWrapper.get())->parentRule());
        ASSERT(!m_cssomWrapper->parentElement());
        return *m_cssomWrapper;
    }
    m_cssomWrapper = makeUnique<PropertySetCSSStyleDeclaration>(*this);
    return *m_cssomWrapper;
}

unsigned StyleProperties::averageSizeInBytes()
{
    // Please update this if the storage scheme changes so that this longer reflects the actual size.
    return ImmutableStyleProperties::objectSize(4);
}

// See the function above if you need to update this.
struct SameSizeAsStyleProperties : public RefCounted<SameSizeAsStyleProperties> {
    unsigned bitfield;
};
static_assert(sizeof(StyleProperties) == sizeof(SameSizeAsStyleProperties), "style property set should stay small");

#ifndef NDEBUG
void StyleProperties::showStyle()
{
    fprintf(stderr, "%s\n", asText().ascii().data());
}
#endif

String StyleProperties::PropertyReference::cssName() const
{
    if (id() == CSSPropertyCustom)
        return downcast<CSSCustomPropertyValue>(*value()).name();
    return nameString(id());
}

String StyleProperties::PropertyReference::cssText() const
{
    return makeString(cssName(), ": ", WebCore::serializeLonghandValue(id(), *m_value), isImportant() ? " !important;" : ";");
}

} // namespace WebCore
