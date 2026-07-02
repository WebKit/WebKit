/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include "CSSString.h"

namespace WebCore {
namespace CSS {

// <'quote'> = auto | none | match-parent | [ <string> <string> ]+
// FIXME: Add support for `match-parent`.
// https://drafts.csswg.org/css-content-3/#propdef-quotes
struct Quotes {
    using Data = SpaceSeparatedVector<String>;

    Quotes(Keyword::Auto keyword)
        : m_value { keyword }
    {
    }

    Quotes(Keyword::None keyword)
        : m_value { keyword }
    {
    }

    Quotes(Data&& data)
        : m_value { WTF::move(data) }
    {
    }

    bool isAuto() const { return WTF::holdsAlternative<Keyword::Auto>(m_value); }
    bool isNone() const { return WTF::holdsAlternative<Keyword::None>(m_value); }
    bool isQuotes() const { return WTF::holdsAlternative<Data>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const Quotes&) const = default;

private:
    Variant<Keyword::Auto, Keyword::None, Data> m_value;
};

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::Quotes)
