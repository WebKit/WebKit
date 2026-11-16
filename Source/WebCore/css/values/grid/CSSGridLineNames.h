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

#include <WebCore/CSSCustomIdent.h>
#include <WebCore/CSSValueTypes.h>

namespace WebCore {
namespace CSS {

// <line-names> = '[' <custom-ident excluding=span,auto>* ']'
// https://drafts.csswg.org/css-grid/#typedef-line-names
struct GridLineNames {
    using Container = SpaceSeparatedVector<CustomIdent>;
    using iterator = typename Container::iterator;
    using reverse_iterator = typename Container::reverse_iterator;
    using const_iterator = typename Container::const_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;
    using value_type = typename Container::value_type;

    Container value;

    iterator begin() LIFETIME_BOUND { return value.begin(); }
    iterator end() LIFETIME_BOUND { return value.end(); }
    reverse_iterator rbegin() LIFETIME_BOUND { return value.rbegin(); }
    reverse_iterator rend() LIFETIME_BOUND { return value.rend(); }

    const_iterator begin() const LIFETIME_BOUND { return value.begin(); }
    const_iterator end() const LIFETIME_BOUND { return value.end(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return value.rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return value.rend(); }

    bool isEmpty() const { return value.isEmpty(); }
    size_t size() const { return value.size(); }
    const value_type& operator[](size_t i) const LIFETIME_BOUND { return value[i]; }

    bool operator==(const GridLineNames&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(GridLineNames, value);

// MARK: - Serialization

// Custom serialization needed to add leading '[' and trailing ']'.
template<> struct Serialize<GridLineNames> { void operator()(StringBuilder&, const SerializationContext&, const GridLineNames&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::GridLineNames)
