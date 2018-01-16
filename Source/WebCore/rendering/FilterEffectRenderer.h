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

namespace WebCore {

class Document;
class FilterEffect;
class FilterOperations;
class GraphicsContext;
class ReferenceFilterOperation;
class RenderElement;
class RenderLayer;
class SourceGraphic;

enum FilterConsumer { FilterProperty, FilterFunction };

class FilterEffectRendererHelper {
    WTF_MAKE_FAST_ALLOCATED;
public:
    FilterEffectRendererHelper(bool haveFilterEffect, GraphicsContext& targetContext);

    bool haveFilterEffect() const { return m_haveFilterEffect; }
    bool hasStartedFilterEffect() const { return m_startedFilterEffect; }

    bool prepareFilterEffect(RenderLayer&, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect);
    bool beginFilterEffect();
    void applyFilterEffect(GraphicsContext& destinationContext);
    
    GraphicsContext* filterContext() const;

    const LayoutRect& repaintRect() const { return m_repaintRect; }

private:
    RenderLayer* m_renderLayer { nullptr }; // FIXME: this is mainly used to get the FilterEffectRenderer. FilterEffectRendererHelper should be weaned off it.
    LayoutPoint m_paintOffset;
    LayoutRect m_repaintRect;
    const GraphicsContext& m_targetContext;
    bool m_haveFilterEffect { false };
    bool m_startedFilterEffect { false };
};

// This is used to render filters for the CSS filter: property, and the filter() image function.
class FilterEffectRenderer final : public Filter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    friend class FilterEffectRendererHelper;

    static Ref<FilterEffectRenderer> create();

    void setSourceImageRect(const FloatRect&);
    void setFilterRegion(const FloatRect& filterRegion) { m_filterRegion = filterRegion; }

    ImageBuffer* output() const;

    bool build(RenderElement&, const FilterOperations&, FilterConsumer);
    void clearIntermediateResults();
    void apply();

    bool hasFilterThatMovesPixels() const { return m_hasFilterThatMovesPixels; }
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const { return m_hasFilterThatShouldBeRestrictedBySecurityOrigin; }

private:
    FilterEffectRenderer();
    virtual ~FilterEffectRenderer();

    FloatRect sourceImageRect() const final { return m_sourceDrawingRegion; }
    FloatRect filterRegion() const final { return m_filterRegion; }

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

    IntRectExtent m_outsets;

    bool m_graphicsBufferAttached { false };
    bool m_hasFilterThatMovesPixels { false };
    bool m_hasFilterThatShouldBeRestrictedBySecurityOrigin { false };
};

inline FilterEffectRendererHelper::FilterEffectRendererHelper(bool haveFilterEffect, GraphicsContext& targetContext)
    : m_targetContext(targetContext)
    , m_haveFilterEffect(haveFilterEffect)
{
}

inline void FilterEffectRenderer::setSourceImageRect(const FloatRect& sourceImageRect)
{
    m_sourceDrawingRegion = sourceImageRect;
    setMaxEffectRects(sourceImageRect);
    setFilterRegion(sourceImageRect);
    m_graphicsBufferAttached = false;
}

} // namespace WebCore
