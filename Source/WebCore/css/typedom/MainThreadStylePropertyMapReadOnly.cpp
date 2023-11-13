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
#include "MainThreadStylePropertyMapReadOnly.h"

#include "CSSPendingSubstitutionValue.h"
#include "CSSProperty.h"
#include "CSSPropertyNames.h"
#include "CSSPropertyParser.h"
#include "CSSStyleValue.h"
#include "CSSStyleValueFactory.h"
#include "CSSTokenizer.h"
#include "CSSUnparsedValue.h"
#include "CSSVariableData.h"
#include "Document.h"
#include "PaintWorkletGlobalScope.h"
#include "StylePropertyShorthand.h"

namespace WebCore {

MainThreadStylePropertyMapReadOnly::MainThreadStylePropertyMapReadOnly() = default;

Document* MainThreadStylePropertyMapReadOnly::documentFromContext(ScriptExecutionContext& context)
{
    ASSERT(isMainThread());
#if ENABLE(CSS_PAINTING_API)
    if (auto* paintWorklet = dynamicDowncast<PaintWorkletGlobalScope>(context))
        return paintWorklet->responsibleDocument();
#endif
    return &downcast<Document>(context);
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-get
ExceptionOr<RefPtr<CSSStyleValue>> MainThreadStylePropertyMapReadOnly::get(ScriptExecutionContext& context, const AtomString& property) const
{
    auto* document = documentFromContext(context);
    if (!document)
        return nullptr;

    if (isCustomPropertyName(property))
        return reifyValue(customPropertyValue(property), std::nullopt, *document);

    auto propertyID = cssPropertyID(property);
    if (!isExposed(propertyID, &document->settings()))
        return Exception { ExceptionCode::TypeError, makeString("Invalid property ", property) };

    if (isShorthand(propertyID))
        return CSSStyleValueFactory::constructStyleValueForShorthandSerialization(shorthandPropertySerialization(propertyID), { *document });

    return reifyValue(propertyValue(propertyID), propertyID, *document);
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-getall
ExceptionOr<Vector<RefPtr<CSSStyleValue>>> MainThreadStylePropertyMapReadOnly::getAll(ScriptExecutionContext& context, const AtomString& property) const
{
    auto* document = documentFromContext(context);
    if (!document)
        return Vector<RefPtr<CSSStyleValue>> { };

    if (isCustomPropertyName(property))
        return reifyValueToVector(customPropertyValue(property), std::nullopt, *document);

    auto propertyID = cssPropertyID(property);
    if (!isExposed(propertyID, &document->settings()))
        return Exception { ExceptionCode::TypeError, makeString("Invalid property ", property) };

    if (isShorthand(propertyID)) {
        if (RefPtr value = CSSStyleValueFactory::constructStyleValueForShorthandSerialization(shorthandPropertySerialization(propertyID), { *document }))
            return Vector<RefPtr<CSSStyleValue>> { WTFMove(value) };
        return Vector<RefPtr<CSSStyleValue>> { };
    }

    return reifyValueToVector(propertyValue(propertyID), propertyID, *document);
}

// https://drafts.css-houdini.org/css-typed-om-1/#dom-stylepropertymapreadonly-has
ExceptionOr<bool> MainThreadStylePropertyMapReadOnly::has(ScriptExecutionContext& context, const AtomString& property) const
{
    auto result = get(context, property);
    if (result.hasException())
        return result.releaseException();
    return !!result.returnValue();
}

} // namespace WebCore
