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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSURLModifiers.h>
#include <wtf/URL.h>

namespace WebCore {

class Document;
struct CSSParserContext;

namespace CSS {

// https://drafts.csswg.org/css-values-4/#url-value
struct URL {
    WTF::String specified;
    WTF::URL resolved;
    URLModifiers modifiers;

    static URL none() { return { .specified = { }, .resolved = { }, .modifiers = { } }; }
    bool isNone() const { return specified.isNull(); }

    bool operator==(const URL&) const = default;
};

template<size_t I> const auto& get(const URL& value)
{
    if constexpr (!I)
        return value.specified;
    if constexpr (I == 1)
        return value.resolved;
    if constexpr (I == 2)
        return value.modifiers;
}

template<> struct Serialize<URL> { void operator()(StringBuilder&, const SerializationContext&, const URL&); };

std::optional<URL> completeURL(const String&, const CSSParserContext&);
std::optional<URL> completeURL(const String&, const Document&);

URL resolve(URL&&);

bool mayDependOnBaseURL(const URL&);

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::URL, 3)
