/*
 * Copyright (C) 2011-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/BoxExtents.h>
#include <WebCore/Filter.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class GraphicsContext;
class RenderElement;

namespace Style {
struct Filter;
struct FilterValue;
}

class CSSFilterRenderer final : public Filter {
    WTF_MAKE_TZONE_ALLOCATED(CSSFilterRenderer);
public:
    static RefPtr<CSSFilterRenderer> create(RenderElement&, const Style::Filter&, const FilterGeometry&, OptionSet<FilterRenderingMode>, bool showDebugOverlay, const GraphicsContext& destinationContext);

    WEBCORE_EXPORT static Ref<CSSFilterRenderer> create(Vector<Ref<FilterFunction>>&&);
    WEBCORE_EXPORT static Ref<CSSFilterRenderer> create(Vector<Ref<FilterFunction>>&&, const FilterGeometry&, OptionSet<FilterRenderingMode> preferredFilterRenderingModes, bool showDebugOverlay);

    const Vector<Ref<FilterFunction>>& functions() const { return m_functions; }
    void setFilterRegion(const FloatRect&);

    bool hasFilterThatMovesPixels() const { return m_hasFilterThatMovesPixels; }
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const { return m_hasFilterThatShouldBeRestrictedBySecurityOrigin; }

    FilterEffectVector effectsOfType(FilterFunction::Type) const final;

    RefPtr<FilterImage> apply(FilterImage* sourceImage, FilterResults&) final;
    FilterStyleVector createFilterStyles(GraphicsContext&, const FilterStyle& sourceStyle) const final;

    static bool isIdentity(RenderElement&, const Style::Filter&);
    static IntOutsets calculateOutsets(RenderElement&, const Style::Filter&, const FloatRect& targetBoundingBox);

private:
    CSSFilterRenderer(const FilterGeometry&, bool hasFilterThatMovesPixels, bool hasFilterThatShouldBeRestrictedBySecurityOrigin);
    CSSFilterRenderer(Vector<Ref<FilterFunction>>&&, const FilterGeometry&);

    RefPtr<FilterFunction> buildFilterFunction(RenderElement&, const Style::FilterValue&, OptionSet<FilterRenderingMode>, const GraphicsContext& destinationContext);
    bool buildFilterFunctions(RenderElement&, const Style::Filter&, OptionSet<FilterRenderingMode>, const GraphicsContext& destinationContext);

    void computeEnclosingFilterRegion();

    OptionSet<FilterRenderingMode> supportedFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes) const final;

    WTF::TextStream& externalRepresentation(WTF::TextStream&, FilterRepresentation) const final;

    Vector<Ref<FilterFunction>> m_functions;
    bool m_hasFilterThatMovesPixels { false };
    bool m_hasFilterThatShouldBeRestrictedBySecurityOrigin { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_FILTER_FUNCTION(CSSFilterRenderer);
