/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StylePropertyMap.h"

#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnparsedValue.h"
#include "CSSValueList.h"
#include "CSSValuePair.h"
#include "CSSVariableReferenceValue.h"
#include "Document.h"
#include "StylePropertyShorthand.h"
#include <wtf/FixedVector.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static RefPtr<CSSValue> cssValueFromStyleValues(CSSPropertyID propertyID, Vector<Ref<CSSStyleValue>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto toCSSValue = [propertyID](CSSStyleValue& styleValue) {
        return styleValue.toCSSValueWithProperty(propertyID);
    };

    if (values.size() == 1)
        return toCSSValue(values[0]);
    CSSValueListBuilder list;
    for (auto&& value : WTFMove(values)) {
        if (auto cssValue = toCSSValue(value))
            list.append(cssValue.releaseNonNull());
    }
    auto separator = CSSProperty::listValuedPropertySeparator(propertyID);
    return CSSValueList::create(separator, WTFMove(list));
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-set
ExceptionOr<void> StylePropertyMap::set(Document& document, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values)
{
    // 1. If property is not a custom property name string, set property to property ASCII lowercased.
    // 2. If property is not a valid CSS property, throw a TypeError.

    // 3. If property is a single-valued property and values has more than one item, throw a TypeError.
    // 4. If any of the items in values have a non-null [[associatedProperty]] internal slot, and that slot’s value is anything other than property, throw a TypeError.
    // 5. If the size of values is two or more, and one or more of the items are a CSSUnparsedValue or CSSVariableReferenceValue object, throw a TypeError.

    // NOTE: Having 2+ values implies that you’re setting multiple items of a list-valued property, but the presence of a var() function in the string-based OM disables all syntax parsing, including splitting into individual iterations (because there might be more commas inside of the var() value, so you can’t tell how many items are actually going to show up). This step’s restriction preserves the same semantics in the Typed OM.
    // 6. Let props be the value of this’s [[declarations]] internal slot.
    // 7. If props[property] exists, remove it.
    // 8. Let values to set be an empty list.
    // 9. For each value in values, create an internal representation for property and value, and append the result to values to set.
    // 10. Set props[property] to values to set.

    if (isCustomPropertyName(property)) {
        auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values), { document });
        if (styleValuesOrException.hasException())
            return styleValuesOrException.releaseException();
        auto styleValues = styleValuesOrException.releaseReturnValue();
        if (styleValues.size() != 1 || !is<CSSUnparsedValue>(styleValues[0].get()))
            return Exception { ExceptionCode::TypeError, "Invalid values"_s };

        auto value = styleValues[0]->toCSSValue();
        if (!value)
            return Exception { ExceptionCode::TypeError, "Invalid values"_s };
        setCustomProperty(document, property, downcast<CSSVariableReferenceValue>(value.releaseNonNull()));
        return { };
    }
    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isExposed(propertyID, document.settings()))
        return Exception { ExceptionCode::TypeError, makeString("Invalid property "_s, property) };

    if (!CSSProperty::isListValuedProperty(propertyID) && values.size() > 1)
        return Exception { ExceptionCode::TypeError, makeString(property, " is not a list-valued property but more than one value was provided"_s) };

    if (isShorthand(propertyID)) {
        if (values.size() != 1)
            return Exception { ExceptionCode::TypeError, "Wrong number of values for shorthand CSS property"_s };
        String value;
        switchOn(values[0], [&](const RefPtr<CSSStyleValue>& styleValue) {
            value = styleValue->toString();
        }, [&](const String& string) {
            value = string;
        });
        if (value.isEmpty() || !setShorthandProperty(propertyID, value))
            return Exception { ExceptionCode::TypeError, "Bad value for shorthand CSS property"_s };
        return { };
    }

    auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values), { document });
    if (styleValuesOrException.hasException())
        return styleValuesOrException.releaseException();
    auto styleValues = styleValuesOrException.releaseReturnValue();
    if (styleValues.size() == 1) {
        if (auto associatedProperty = styleValues[0]->associatedProperty(); associatedProperty && *associatedProperty != propertyID)
            return Exception { ExceptionCode::TypeError, "CSSStyleValue has an [[associatedProperty]] that does not match the property being set."_s };
    } else if (styleValues.size() > 1) {
        for (auto& styleValue : styleValues) {
            if (auto associatedProperty = styleValue->associatedProperty(); associatedProperty && *associatedProperty != propertyID)
                return Exception { ExceptionCode::TypeError, "CSSStyleValue has an [[associatedProperty]] that does not match the property being set."_s };

            if (is<CSSUnparsedValue>(styleValue.get()))
                return Exception { ExceptionCode::TypeError, "There is more than one value and one is either a CSSVariableReferenceValue or a CSSUnparsedValue"_s };
        }
    }
    auto value = cssValueFromStyleValues(propertyID, WTFMove(styleValues));
    if (!value)
        return Exception { ExceptionCode::TypeError, "Invalid values"_s };

    // The CSS Parser may silently convert number values to lengths. However, CSS Typed OM doesn't allow this so
    // we do some pre-validation.
    // FIXME: Eventually, we should be able to generate most of the validation code and not rely on the CSS parser
    // at all.
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(*value); primitiveValue && primitiveValue->isNumberOrInteger()) {
        if (!CSSProperty::allowsNumberOrIntegerInput(propertyID))
            return Exception { ExceptionCode::TypeError, "Invalid value: This property doesn't allow <number> input"_s };
    }

    if (!setProperty(propertyID, value.releaseNonNull()))
        return Exception { ExceptionCode::TypeError, "Invalid values"_s };

    return { };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-append
ExceptionOr<void> StylePropertyMap::append(Document& document, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values)
{
    if (values.isEmpty())
        return { };

    if (isCustomPropertyName(property))
        return Exception { ExceptionCode::TypeError, "Cannot append to custom properties"_s };

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isExposed(propertyID, document.settings()))
        return Exception { ExceptionCode::TypeError, makeString("Invalid property "_s, property) };

    if (!CSSProperty::isListValuedProperty(propertyID))
        return Exception { ExceptionCode::TypeError, makeString(property, " does not support multiple values"_s) };

    auto currentValue = propertyValue(propertyID);
    CSSValueListBuilder list;
    if (auto* currentList = dynamicDowncast<CSSValueList>(currentValue.get()))
        list = currentList->copyValues();
    else if (currentValue)
        list.append(currentValue.releaseNonNull());

    auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values), { document });
    if (styleValuesOrException.hasException())
        return styleValuesOrException.releaseException();

    auto styleValues = styleValuesOrException.releaseReturnValue();
    for (auto& styleValue : styleValues) {
        if (is<CSSUnparsedValue>(styleValue.get()))
            return Exception { ExceptionCode::TypeError, "Values cannot contain a CSSVariableReferenceValue or a CSSUnparsedValue"_s };
        if (auto cssValue = styleValue->toCSSValueWithProperty(propertyID))
            list.append(cssValue.releaseNonNull());
    }

    if (!setProperty(propertyID, CSSValueList::create(CSSProperty::listValuedPropertySeparator(propertyID), WTFMove(list))))
        return Exception { ExceptionCode::TypeError, "Invalid values"_s };

    return { };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-delete
ExceptionOr<void> StylePropertyMap::remove(Document& document, const AtomString& property)
{
    if (isCustomPropertyName(property)) {
        removeCustomProperty(property);
        return { };
    }

    auto propertyID = cssPropertyID(property);
    if (!isExposed(propertyID, document.settings()))
        return Exception { ExceptionCode::TypeError, makeString("Invalid property "_s, property) };

    removeProperty(propertyID);
    return { };
}

} // namespace WebCore
