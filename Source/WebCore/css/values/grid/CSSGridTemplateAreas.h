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

#include "CSSValueTypes.h"
#include "GridArea.h"

namespace WebCore {
namespace CSS {

// Parsed representation of the `<string>+` of <'grid-template-areas'>.
struct NamedGridAreaMap {
    using Map = UncheckedKeyHashMap<String, GridArea>;

    Map map;
    size_t rowCount { 0 };
    size_t columnCount { 0 };

    bool operator==(const NamedGridAreaMap&) const = default;
};

// <'grid-template-areas'> = none | <string>+
// https://drafts.csswg.org/css-grid/#propdef-grid-template-areas
struct GridTemplateAreas {
    NamedGridAreaMap map;

    GridTemplateAreas(CSS::Keyword::None)
        : map { }
    {
    }

    GridTemplateAreas(const NamedGridAreaMap& map)
        : map { map }
    {
    }

    GridTemplateAreas(NamedGridAreaMap&& map)
        : map { WTFMove(map) }
    {
    }

    bool isNone() const { return !map.rowCount; }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        if (!map.rowCount)
            return functor(CSS::Keyword::None { });
        return functor(map);
    }

    bool operator==(const GridTemplateAreas&) const = default;
};


template<> struct Serialize<NamedGridAreaMap> { void operator()(StringBuilder&, const CSS::SerializationContext&, const NamedGridAreaMap&); };
template<> struct CSSValueCreation<NamedGridAreaMap> { Ref<CSSValue> operator()(CSSValuePool&, const NamedGridAreaMap&); };

} // namespace CSS
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::CSS::GridTemplateAreas> = true;
