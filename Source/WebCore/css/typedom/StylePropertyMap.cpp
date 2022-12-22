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
#include "CSSVariableReferenceValue.h"
#include "Document.h"
#include "StylePropertyShorthand.h"
#include <wtf/FixedVector.h>

namespace WebCore {

static RefPtr<CSSValue> cssValueFromStyleValues(std::optional<CSSPropertyID> propertyID, Vector<Ref<CSSStyleValue>>&& values)
{
    if (values.isEmpty())
        return nullptr;

    auto toCSSValue = [propertyID](CSSStyleValue& styleValue) {
        if (propertyID)
            return styleValue.toCSSValueWithProperty(*propertyID);
        return styleValue.toCSSValue();
    };
    if (values.size() == 1)
        return toCSSValue(values[0]);
    auto list = propertyID ? CSSProperty::createListForProperty(*propertyID) : CSSValueList::createCommaSeparated();
    for (auto&& value : WTFMove(values)) {
        if (auto cssValue = toCSSValue(value))
            list->append(cssValue.releaseNonNull());
    }
    return list;
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-set
ExceptionOr<void> StylePropertyMap::set(Document& document, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values)
{
    if (isCustomPropertyName(property)) {
        auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values));
        if (styleValuesOrException.hasException())
            return styleValuesOrException.releaseException();
        auto styleValues = styleValuesOrException.releaseReturnValue();
        if (styleValues.size() != 1 || !is<CSSUnparsedValue>(styleValues[0].get()))
            return Exception { TypeError, "Invalid values"_s };

        auto value = styleValues[0]->toCSSValue();
        if (!value)
            return Exception { TypeError, "Invalid values"_s };
        setCustomProperty(document, property, static_reference_cast<CSSVariableReferenceValue>(value.releaseNonNull()));
        return { };
    }
    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isExposed(propertyID, document.settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (!CSSProperty::isListValuedProperty(propertyID) && values.size() > 1)
        return Exception { TypeError, makeString(property, " is not a list-valued property but more than one value was provided"_s) };

    if (isShorthandCSSProperty(propertyID)) {
        if (values.size() != 1)
            return Exception { TypeError, "Wrong number of values for shorthand CSS property"_s };
        String value;
        switchOn(values[0], [&](const RefPtr<CSSStyleValue>& styleValue) {
            value = styleValue->toString();
        }, [&](const String& string) {
            value = string;
        });
        if (value.isEmpty() || !setShorthandProperty(propertyID, value))
            return Exception { TypeError, "Bad value for shorthand CSS property"_s };
        return { };
    }

    auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values));
    if (styleValuesOrException.hasException())
        return styleValuesOrException.releaseException();
    auto styleValues = styleValuesOrException.releaseReturnValue();
    if (styleValues.size() > 1) {
        for (auto& styleValue : styleValues) {
            if (is<CSSUnparsedValue>(styleValue.get()))
                return Exception { TypeError, "There is more than one value and one is either a CSSVariableReferenceValue or a CSSUnparsedValue"_s };
        }
    }
    auto value = cssValueFromStyleValues(propertyID, WTFMove(styleValues));
    if (!value)
        return Exception { TypeError, "Invalid values"_s };

    // The CSS Parser may silently convert number values to lengths. However, CSS Typed OM doesn't allow this so
    // we do some pre-validation.
    // FIXME: Eventually, we should be able to generate most of the validation code and not rely on the CSS parser
    // at all.
    if (auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(*value); primitiveValue && primitiveValue->isNumberOrInteger()) {
        if (!CSSProperty::allowsNumberOrIntegerInput(propertyID))
            return Exception { TypeError, "Invalid value: This property doesn't allow <number> input"_s };
    }

    if (!setProperty(propertyID, value.releaseNonNull()))
        return Exception { TypeError, "Invalid values"_s };

    return { };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-append
ExceptionOr<void> StylePropertyMap::append(Document& document, const AtomString& property, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&& values)
{
    if (values.isEmpty())
        return { };

    if (isCustomPropertyName(property))
        return Exception { TypeError, "Cannot append to custom properties"_s };

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isExposed(propertyID, document.settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (!CSSProperty::isListValuedProperty(propertyID))
        return Exception { TypeError, makeString(property, " does not support multiple values"_s) };

    RefPtr<CSSValueList> list;
    auto currentValue = propertyValue(propertyID);
    if (!currentValue || !is<CSSValueList>(*currentValue)) {
        list = CSSProperty::createListForProperty(propertyID);
        if (currentValue)
            list->append(currentValue.releaseNonNull());
    } else
        list = &downcast<CSSValueList>(*currentValue);

    auto styleValuesOrException = CSSStyleValueFactory::vectorFromStyleValuesOrStrings(property, WTFMove(values));
    if (styleValuesOrException.hasException())
        return styleValuesOrException.releaseException();

    auto styleValues = styleValuesOrException.releaseReturnValue();
    for (auto& styleValue : styleValues) {
        if (is<CSSUnparsedValue>(styleValue.get()))
            return Exception { TypeError, "Values cannot contain a CSSVariableReferenceValue or a CSSUnparsedValue"_s };
        if (auto cssValue = styleValue->toCSSValue())
            list->append(cssValue.releaseNonNull());
    }

    if (!setProperty(propertyID, list.releaseNonNull()))
        return Exception { TypeError, "Invalid values"_s };

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
        return Exception { TypeError, makeString("Invalid property ", property) };

    removeProperty(propertyID);
    return { };
}

} // namespace WebCore
