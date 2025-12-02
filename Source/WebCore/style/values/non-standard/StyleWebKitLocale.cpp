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
#include "StyleWebKitLocale.h"

#include "StyleBuilderChecking.h"

namespace WebCore {
namespace Style {

// MARK: - Conversion

auto CSSValueConversion<WebkitLocale>::operator()(BuilderState& state, const CSSValue& value) -> WebkitLocale
{
    RefPtr primitiveValue = requiredDowncast<CSSPrimitiveValue>(state, value);
    if (!primitiveValue)
        return CSS::Keyword::Auto { };

    switch (primitiveValue->valueID()) {
    case CSSValueInvalid:
        return AtomString { primitiveValue->stringValue() };
    case CSSValueAuto:
        return CSS::Keyword::Auto { };
    default:
        state.setCurrentPropertyInvalidAtComputedValueTime();
        return CSS::Keyword::Auto { };
    }
}

} // namespace Style
} // namespace WebCore
