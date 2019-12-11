/*
 * Copyright (C) 2011-2017 Apple Inc. All rights reserved.
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

#include "Filter.h"
#include "IntRectExtent.h"
#include "LayoutRect.h"
#include <wtf/TypeCasts.h>

namespace WebCore {

class FilterEffect;
class FilterOperations;
class GraphicsContext;
class ReferenceFilterOperation;
class RenderElement;
class SourceAlpha;
class SourceGraphic;

enum class FilterConsumer { FilterProperty, FilterFunction };

class CSSFilter final : public Filter {
    WTF_MAKE_FAST_ALLOCATED;
    friend class RenderLayerFilters;
public:
    static Ref<CSSFilter> create();

    void setSourceImageRect(const FloatRect&);
    void setFilterRegion(const FloatRect& filterRegion) { m_filterRegion = filterRegion; }

    ImageBuffer* output() const;

    bool build(RenderElement&, const FilterOperations&, FilterConsumer);
    void clearIntermediateResults();
    void apply();

    bool hasFilterThatMovesPixels() const { return m_hasFilterThatMovesPixels; }
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const { return m_hasFilterThatShouldBeRestrictedBySecurityOrigin; }

    void determineFilterPrimitiveSubregion();
    IntOutsets outsets() const;

private:
    CSSFilter();
    virtual ~CSSFilter();

    bool isCSSFilter() const final { return true; }

    FloatRect sourceImageRect() const final { return m_sourceDrawingRegion; }

    FloatRect filterRegion() const final { return m_filterRegion; }
    FloatRect filterRegionInUserSpace() const final { return m_filterRegion; }

    RefPtr<FilterEffect> buildReferenceFilter(RenderElement&, FilterEffect& previousEffect, ReferenceFilterOperation&);

    void setMaxEffectRects(const FloatRect&);

    GraphicsContext* inputContext();

    bool updateBackingStoreRect(const FloatRect& filterRect);
    void allocateBackingStoreIfNeeded(const GraphicsContext&);

    IntRect outputRect() const;

    LayoutRect computeSourceImageRectForDirtyRect(const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect);

    FloatRect m_sourceDrawingRegion;
    FloatRect m_filterRegion;

    Vector<Ref<FilterEffect>> m_effects;
    Ref<SourceGraphic> m_sourceGraphic;
    RefPtr<FilterEffect> m_sourceAlpha;

    mutable IntOutsets m_outsets;

    bool m_graphicsBufferAttached { false };
    bool m_hasFilterThatMovesPixels { false };
    bool m_hasFilterThatShouldBeRestrictedBySecurityOrigin { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CSSFilter)
    static bool isType(const WebCore::Filter& filter) { return filter.isCSSFilter(); }
SPECIALIZE_TYPE_TRAITS_END()
