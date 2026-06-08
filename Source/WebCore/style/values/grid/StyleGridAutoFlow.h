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

#include <WebCore/CSSGridAutoFlow.h>
#include <WebCore/StyleValueTypes.h>

namespace WebCore {
namespace Style {

// FIXME: Style::GridAutoFlow and CSS::GridAutoFlow are identical and ideally would
// be the same type using TreatAsNonConverting<>, but currently TreatAsNonConverting<>
// cannot be used on types that are TreatAsVariantLike<>, which CSS::GridAutoFlow is.

// <'grid-auto-flow'> = normal | [ [ row | column ] || dense ]
// FIXME: `normal` is not specified in the link below. Figure out where `normal` comes from and add link.
// https://drafts.csswg.org/css-grid/#grid-auto-flow-property
struct GridAutoFlow : CSS::GridAutoFlow {
    using CSS::GridAutoFlow::GridAutoFlow;

    GridAutoFlow(CSS::GridAutoFlow base)
        : CSS::GridAutoFlow { base }
    {
    }
};

// MARK: - Conversion

template<> struct CSSValueConversion<GridAutoFlow> { GridAutoFlow NODELETE operator()(BuilderState&, const CSSValue&); };
template<> struct CSSValueCreation<GridAutoFlow> { Ref<CSSValue> operator()(CSSValuePool&, const RenderStyle&, const GridAutoFlow&); };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridAutoFlow)
