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

#include "CSSPendingSubstitutionValue.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSStyleValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSUnparsedValue.h"
#include "CSSValueList.h"
#include "CSSVariableData.h"
#include "Document.h"
#include "PaintWorkletGlobalScope.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

Document* StylePropertyMap::documentFromContext(ScriptExecutionContext& context)
{
    ASSERT(isMainThread());
#if ENABLE(CSS_PAINTING_API)
    if (auto* paintWorklet = dynamicDowncast<PaintWorkletGlobalScope>(context))
        return paintWorklet->responsibleDocument();
#endif
    return &downcast<Document>(context);
}

RefPtr<CSSStyleValue> StylePropertyMap::shorthandPropertyValue(Document& document, CSSPropertyID propertyID) const
{
    auto shorthand = shorthandForProperty(propertyID);
    Vector<Ref<CSSValue>> values;
    for (auto& longhand : shorthand) {
        RefPtr value = propertyValue(longhand);
        if (!value)
            return nullptr;
        if (value->isImplicitInitialValue())
            continue;
        // VariableReference set from the shorthand.
        if (is<CSSPendingSubstitutionValue>(*value))
            return CSSUnparsedValue::create(downcast<CSSPendingSubstitutionValue>(*value).shorthandValue().data().tokenRange());
        values.append(value.releaseNonNull());
    }
    if (values.isEmpty())
        return nullptr;

    if (values.size() == 1)
        return reifyValue(values[0].ptr(), document);

    auto valueList = CSSValueList::createSpaceSeparated();
    for (auto& value : values)
        valueList->append(WTFMove(value));
    return CSSStyleValue::create(WTFMove(valueList));
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-get
ExceptionOr<RefPtr<CSSStyleValue>> StylePropertyMap::get(ScriptExecutionContext& context, const AtomString& property) const
{
    auto* document = documentFromContext(context);
    if (!document)
        return nullptr;

    if (isCustomPropertyName(property))
        return reifyValue(customPropertyValue(property), *document);

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isCSSPropertyExposed(propertyID, &document->settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (isShorthandCSSProperty(propertyID))
        return shorthandPropertyValue(*document, propertyID);

    return reifyValue(propertyValue(propertyID), *document);
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-getall
ExceptionOr<Vector<RefPtr<CSSStyleValue>>> StylePropertyMap::getAll(ScriptExecutionContext& context, const AtomString& property) const
{
    auto* document = documentFromContext(context);
    if (!document)
        return Vector<RefPtr<CSSStyleValue>> { };

    if (isCustomPropertyName(property))
        return reifyValueToVector(customPropertyValue(property), *document);

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isCSSPropertyExposed(propertyID, &document->settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    if (isShorthandCSSProperty(propertyID)) {
        if (RefPtr value = shorthandPropertyValue(*document, propertyID))
            return Vector<RefPtr<CSSStyleValue>> { WTFMove(value) };
        return Vector<RefPtr<CSSStyleValue>> { };
    }

    return reifyValueToVector(propertyValue(propertyID), *document);
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-has
ExceptionOr<bool> StylePropertyMap::has(ScriptExecutionContext& context, const AtomString& property) const
{
    auto result = get(context, property);
    if (result.hasException())
        return result.releaseException();
    return !!result.returnValue();
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-set
ExceptionOr<void> StylePropertyMap::set(Document&, const AtomString&, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&&)
{
    return Exception { NotSupportedError, "set() is not yet supported"_s };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-append
ExceptionOr<void> StylePropertyMap::append(Document&, const AtomString&, FixedVector<std::variant<RefPtr<CSSStyleValue>, String>>&&)
{
    return Exception { NotSupportedError, "append() is not yet supported"_s };
}

// https://drafts.css-houdini.org/css-typed-om/#dom-stylepropertymap-delete
ExceptionOr<void> StylePropertyMap::remove(Document& document, const AtomString& property)
{
    if (isCustomPropertyName(property)) {
        removeCustomProperty(property);
        return { };
    }

    auto propertyID = cssPropertyID(property);
    if (propertyID == CSSPropertyInvalid || !isCSSPropertyExposed(propertyID, &document.settings()))
        return Exception { TypeError, makeString("Invalid property ", property) };

    removeProperty(propertyID);
    return { };
}

} // namespace WebCore
