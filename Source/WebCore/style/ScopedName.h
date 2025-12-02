/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleScopeOrdinal.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/text/AtomString.h>

namespace WebCore {
namespace Style {

struct ScopedName {
    AtomString name;
    ScopeOrdinal scopeOrdinal { ScopeOrdinal::Element };
    bool isIdentifier { true };

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);
        if (isIdentifier)
            return visitor(CustomIdentifier { name });
        return visitor(name);
    }

    bool operator==(const ScopedName&) const = default;
};

// MARK: - Conversion

template<> struct CSSValueConversion<ScopedName> {
    ScopedName operator()(BuilderState&, const CSSPrimitiveValue&);
    ScopedName operator()(BuilderState&, const CSSValue&);
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const ScopedName&);

} // namespace Style
} // namespace WebCore

namespace WTF {

template<>
struct MarkableTraits<WebCore::Style::ScopedName> {
    static bool isEmptyValue(const WebCore::Style::ScopedName& value) { return value.name.isNull(); }
    static WebCore::Style::ScopedName emptyValue() { return WebCore::Style::ScopedName { nullAtom() }; }
};

} // namespace WTF

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::ScopedName)
