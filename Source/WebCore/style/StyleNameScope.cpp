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
#include "StyleNameScope.h"

#include "CSSCustomIdentValue.h"
#include "CSSKeywordValue.h"
#include "StyleBuilderChecking.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

auto CSSValueConversion<NameScope>::operator()(BuilderState& state, const CSSValue& value) -> NameScope
{
    if (auto* keywordValue = dynamicDowncast<CSSKeywordValue>(value)) {
        switch (keywordValue->valueID()) {
        case CSSValueNone:
            return CSS::Keyword::None { };
        case CSSValueAll:
            return { CSS::Keyword::All { }, state.styleScopeOrdinal() };
        default:
            state.setCurrentPropertyInvalidAtComputedValueTime();
            return CSS::Keyword::None { };
        }
    }

    if (RefPtr customIdentValue = dynamicDowncast<CSSCustomIdentValue>(value))
        return { CommaSeparatedListHashSet<CustomIdent> { toStyleFromCSSValue<CustomIdent>(state, *customIdentValue) }, state.styleScopeOrdinal() };

    auto list = requiredListDowncast<CSSValueList, CSSCustomIdentValue>(state, value);
    if (!list)
        return CSS::Keyword::None { };

    return {
        CommaSeparatedListHashSet<CustomIdent>::map(*list, [&](auto& item) {
            return toStyleFromCSSValue<CustomIdent>(state, item);
        }),
        state.styleScopeOrdinal()
    };
}

} // namespace Style
} // namespace WebCore
