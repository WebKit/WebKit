/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "FilterRenderingMode.h"
#include "RenderLayer.h"

namespace WebCore {

class CachedSVGDocument;
class Element;
class FilterOperations;

class RenderLayerFilters final : private CachedSVGDocumentClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit RenderLayerFilters(RenderLayer&);
    virtual ~RenderLayerFilters();

    const LayoutRect& dirtySourceRect() const { return m_dirtySourceRect; }
    void expandDirtySourceRect(const LayoutRect& rect) { m_dirtySourceRect.unite(rect); }

    CSSFilter* filter() const { return m_filter.get(); }
    void clearFilter() { m_filter = nullptr; }
    
    bool hasFilterThatMovesPixels() const;
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;

    void updateReferenceFilterClients(const FilterOperations&);
    void removeReferenceFilterClients();

    void setPreferredFilterRenderingModes(OptionSet<FilterRenderingMode> preferredFilterRenderingModes) { m_preferredFilterRenderingModes = preferredFilterRenderingModes; }
    void setFilterScale(const FloatSize& filterScale) { m_filterScale = filterScale; }

    static bool isIdentity(RenderElement&);
    static IntOutsets calculateOutsets(RenderElement&, const FloatRect& targetBoundingBox);

    // Per render
    LayoutRect repaintRect() const { return m_repaintRect; }

    GraphicsContext* beginFilterEffect(RenderElement&, GraphicsContext&, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect);
    void applyFilterEffect(GraphicsContext& destinationContext);

private:
    void notifyFinished(CachedResource&, const NetworkLoadMetrics&) final;
    void resetDirtySourceRect() { m_dirtySourceRect = LayoutRect(); }
    GraphicsContext* inputContext();
    void allocateBackingStoreIfNeeded(GraphicsContext&);

    RenderLayer& m_layer;
    OptionSet<FilterRenderingMode> m_preferredFilterRenderingModes { FilterRenderingMode::Software };
    FloatSize m_filterScale { 1, 1 };

    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;

    LayoutRect m_targetBoundingBox;
    FloatRect m_filterRegion;
    RefPtr<ImageBuffer> m_sourceImage;
    RefPtr<CSSFilter> m_filter;
    LayoutRect m_dirtySourceRect;
    
    // Data used per paint
    LayoutPoint m_paintOffset;
    LayoutRect m_repaintRect;
};

} // namespace WebCore
