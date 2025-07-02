/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
#include "ScopedName.h"

#include "CSSPrimitiveValue.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion

//    template<> struct WebCore::Style::CSSValueConversion<StyleType> {
//                   StyleType operator()(BuilderState&, const CSSValue&);
//        [optional] StyleType operator()(BuilderState&, const CSSPrimitiveValue&);
//    };

auto CSSValueConversion<ScopedName>::operator()(BuilderState& state, const CSSPrimitiveValue& primitiveValue) -> ScopedName
{
    if (!primitiveValue.isCustomIdent()) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { };
    }

    return {
        .name = AtomString { primitiveValue.customIdent() },
        .scopeOrdinal = state.styleScopeOrdinal()
    };
}

auto CSSValueConversion<ScopedName>::operator()(BuilderState& state, const CSSValue& value) -> ScopedName
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return { };

    if (!primitiveValue->isCustomIdent()) {
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return { };
    }

    return ScopedName {
        .name = AtomString { primitiveValue->customIdent() },
        .scopeOrdinal = state.styleScopeOrdinal()
    };
}

auto CSSValueCreation<ScopedName>::operator()(CSSValuePool&, const RenderStyle&, const ScopedName& scopedName) -> Ref<CSSValue>
{
    if (scopedName.isIdentifier)
        return CSSPrimitiveValue::createCustomIdent(scopedName.name);
    return CSSPrimitiveValue::create(scopedName.name);
}

// MARK: - Serialization

void Serialize<ScopedName>::operator()(StringBuilder& builder, const CSS::SerializationContext& context, const RenderStyle& style, const ScopedName& scopedName)
{
    if (scopedName.isIdentifier)
        serializationForCSS(builder, context, style, CustomIdentifier { scopedName.name });
    else
        serializationForCSS(builder, context, style, scopedName.name);
}

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream& ts, const ScopedName& scopedName)
{
    ts << scopedName.name;
    return ts;
}

} // namespace Style
} // namespace WebCore
