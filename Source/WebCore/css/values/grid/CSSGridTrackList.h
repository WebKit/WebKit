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

#include <WebCore/CSSGridLineNames.h>
#include <WebCore/CSSGridTrackSize.h>
#include <WebCore/CSSPrimitiveNumericTypes.h>

namespace WebCore {
namespace CSS {

// <track-repeat>        = repeat( [ <integer [1,∞]> ] , [ <line-names>? <track-size> ]+ <line-names>? )
// <auto-repeat>         = repeat( [ auto-fill | auto-fit ] , [ <line-names>? <fixed-size> ]+ <line-names>? )
// <fixed-repeat>        = repeat( [ <integer [1,∞]> ] , [ <line-names>? <fixed-size> ]+ <line-names>? )
// https://drafts.csswg.org/css-grid/#typedef-track-repeat
struct GridTrackRepeatFunctionParameters {
    using Repetitions = Variant<Integer<Positive, unsigned>, Keyword::AutoFill, Keyword::AutoFit>;
    using Repeated = Variant<GridLineNames, GridTrackSize>;

    Repetitions repetitions;
    SpaceSeparatedVector<Repeated> repeated;

    bool operator==(const GridTrackRepeatFunctionParameters&) const = default;
};
using GridTrackRepeatFunction = FunctionNotation<CSSValueRepeat, GridTrackRepeatFunctionParameters>;

template<size_t I> const auto& get(const GridTrackRepeatFunctionParameters& value)
{
    if constexpr (!I)
        return value.repetitions;
    else if constexpr (I == 1)
        return value.repeated;
}

// <track-list>          = [ <line-names>? [ <track-size> | <track-repeat> ] ]+ <line-names>?
// <auto-track-list>     = [ <line-names>? [ <fixed-size> | <fixed-repeat> ] ]* <line-names>? <auto-repeat>
//                         [ <line-names>? [ <fixed-size> | <fixed-repeat> ] ]* <line-names>?
// https://drafts.csswg.org/css-grid/#typedef-track-list
struct GridTrackList {
    using Track = Variant<GridLineNames, GridTrackSize, GridTrackRepeatFunction>;
    using Container = SpaceSeparatedVector<Track>;
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

    bool operator==(const GridTrackList&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(GridTrackList, value);

} // namespace CSS
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::GridTrackRepeatFunctionParameters, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE_FOR_TYPE_WRAPPER(WebCore::CSS::GridTrackList)
