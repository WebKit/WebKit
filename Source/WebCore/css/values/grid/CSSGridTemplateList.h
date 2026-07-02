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

#include <WebCore/CSSGridSubgrid.h>
#include <WebCore/CSSGridTrackList.h>

namespace WebCore {
namespace CSS {

// <'grid-template-columns'/'grid-template-rows'> = none | <track-list> | <auto-track-list> | subgrid <line-name-list>?
// https://drafts.csswg.org/css-grid/#propdef-grid-template-columns
// https://drafts.csswg.org/css-grid/#propdef-grid-template-rows
struct GridTemplateList {
    GridTemplateList(Keyword::None keyword) : m_value { keyword } { }
    GridTemplateList(GridTrackList&& trackList) : m_value { WTF::move(trackList) } { }
    GridTemplateList(GridSubgrid&& subgrid) : m_value { WTF::move(subgrid) } { }

    bool isNone() const { return WTF::holdsAlternative<Keyword::None>(m_value); }
    bool isTrackList() const { return WTF::holdsAlternative<GridTrackList>(m_value); }
    bool isSubgrid() const { return WTF::holdsAlternative<GridSubgrid>(m_value); }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        return WTF::switchOn(m_value, std::forward<F>(f)...);
    }

    bool operator==(const GridTemplateList&) const = default;

private:
    Variant<Keyword::None, GridTrackList, GridSubgrid> m_value;
};

// Customization is needed to return CSSGridTemplateListValue.
template<> struct CSSValueCreation<GridTemplateList> { Ref<CSSValue> operator()(CSSValuePool&, const GridTemplateList&); };

} // namespace CSS
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::CSS::GridTemplateList)
