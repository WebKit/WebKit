/*
 * Copyright (C) 2025-2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/CachedSVGDocumentReference.h>
#include <WebCore/StyleURL.h>

namespace WebCore {

class CachedResourceLoader;
class FilterOperation;
struct ResourceLoaderOptions;

namespace CSS {
struct FilterReference;
}

namespace Style {

// https://drafts.fxtf.org/filter-effects/#typedef-filter-url
struct FilterReference {
    URL url;

    AtomString cachedFragment;
    RefPtr<CachedSVGDocumentReference> cachedSVGDocumentReference;

    // `FilterReference` is never interpolated. This only exists to allow the generic blending code to compile.
    static FilterReference passthroughForInterpolation() { RELEASE_ASSERT_NOT_REACHED(); }

    constexpr bool requiresRepaintForCurrentColorChange() const { return false; }
    constexpr bool affectsOpacity() const { return true; }
    constexpr bool movesPixels() const { return true; }
    // FIXME: This only needs to return true for graphs that include ConvolveMatrix, DisplacementMap, Morphology and possibly Lighting. https://bugs.webkit.org/show_bug.cgi?id=171753
    constexpr bool shouldBeRestrictedBySecurityOrigin() const { return true; }

    void loadExternalDocumentIfNeeded(CachedResourceLoader&, const ResourceLoaderOptions&);

    // Override's operator-> to allow it to be used in generic contexts with filter functions.
    const FilterReference* operator->() const { return this; }
    FilterReference* operator->() { return this; }

    bool operator==(const FilterReference& other) const
    {
        return url == other.url;
    }
};
DEFINE_TYPE_WRAPPER_GET(FilterReference, url);

// MARK: - Blending

// `FilterReference` is never interpolated. This only exists to allow the generic blending code to compile.
template<> struct Blending<FilterReference> {
    auto blend(const FilterReference&, const FilterReference&, const BlendingContext&) -> FilterReference { RELEASE_ASSERT_NOT_REACHED(); }
};

// MARK: - Conversion

template<> struct ToCSS<FilterReference> { auto operator()(const FilterReference&, const RenderStyle&) -> CSS::FilterReference; };
template<> struct ToStyle<CSS::FilterReference> { auto operator()(const CSS::FilterReference&, const BuilderState&) -> FilterReference; };

} // namespace Style
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::Style::FilterReference, 1)
