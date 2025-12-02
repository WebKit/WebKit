/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class CSSFilterRenderer;
class CachedSVGDocument;
class Element;
class FilterOperations;
class GraphicsContextSwitcher;

class RenderLayerFilters final : public RefCounted<RenderLayerFilters>, private CachedSVGDocumentClient {
    WTF_MAKE_TZONE_ALLOCATED(RenderLayerFilters);
public:
    static Ref<RenderLayerFilters> create(RenderLayer&);
    virtual ~RenderLayerFilters();

    void detachFromLayer() { m_layer = nullptr; }

    // CachedResourceClient.
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    const LayoutRect& dirtySourceRect() const { return m_dirtySourceRect; }
    void expandDirtySourceRect(const LayoutRect& rect) { m_dirtySourceRect.unite(rect); }

    CSSFilterRenderer* filter() const { return m_filter.get(); }
    void clearFilter() { m_filter = nullptr; }
    
    bool hasFilterThatMovesPixels() const;
    bool hasFilterThatShouldBeRestrictedBySecurityOrigin() const;
    bool hasSourceImage() const;

    void updateReferenceFilterClients(const Style::Filter&);
    void removeReferenceFilterClients();

    void setFilterScale(const FloatSize& filterScale) { m_filterScale = filterScale; }

    static bool isIdentity(RenderElement&);
    static IntOutsets calculateOutsets(RenderElement&, const FloatRect& targetBoundingBox);

    // Per render
    LayoutRect repaintRect() const { return m_repaintRect; }

    GraphicsContext* beginFilterEffect(RenderElement&, GraphicsContext&, const LayoutRect& filterBoxRect, const LayoutRect& dirtyRect, const LayoutRect& layerRepaintRect, const LayoutRect& clipRect);
    void applyFilterEffect(GraphicsContext& destinationContext);

private:
    explicit RenderLayerFilters(RenderLayer&);

    void notifyFinished(CachedResource&, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess) final;
    void resetDirtySourceRect() { m_dirtySourceRect = LayoutRect(); }

    CheckedPtr<RenderLayer> m_layer;
    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;

    LayoutRect m_targetBoundingBox;
    LayoutRect m_dirtySourceRect;
    LayoutRect m_repaintRect;

    OptionSet<FilterRenderingMode> m_preferredFilterRenderingModes { FilterRenderingMode::Software };
    FloatSize m_filterScale { 1, 1 };
    FloatRect m_filterRegion;

    RefPtr<CSSFilterRenderer> m_filter;
    std::unique_ptr<GraphicsContextSwitcher> m_targetSwitcher;
};

} // namespace WebCore
