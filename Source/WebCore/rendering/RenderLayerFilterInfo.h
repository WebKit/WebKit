/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RenderLayerFilterInfo_h
#define RenderLayerFilterInfo_h

#if ENABLE(CSS_FILTERS)

#include "CachedResourceHandle.h"
#include "CachedSVGDocumentClient.h"
#include "RenderLayer.h"

namespace WebCore {

class Element;

class RenderLayer::FilterInfo
#if ENABLE(SVG)
    final : public CachedSVGDocumentClient
#endif
{
public:
    static FilterInfo& get(RenderLayer&);
    static FilterInfo* getIfExists(const RenderLayer&);
    static void remove(RenderLayer&);

    const LayoutRect& dirtySourceRect() const { return m_dirtySourceRect; }
    void expandDirtySourceRect(const LayoutRect& rect) { m_dirtySourceRect.unite(rect); }
    void resetDirtySourceRect() { m_dirtySourceRect = LayoutRect(); }
    
    FilterEffectRenderer* renderer() const { return m_renderer.get(); }
    void setRenderer(PassRefPtr<FilterEffectRenderer>);
    
#if ENABLE(SVG)
    void updateReferenceFilterClients(const FilterOperations&);
    void removeReferenceFilterClients();
#endif

private:
    explicit FilterInfo(RenderLayer&);
    ~FilterInfo();

    friend void WTF::deleteOwnedPtr<FilterInfo>(FilterInfo*);

#if ENABLE(SVG)
    virtual void notifyFinished(CachedResource*) override;
#endif

    static HashMap<const RenderLayer*, OwnPtr<FilterInfo>>& map();

#if PLATFORM(IOS)
#pragma clang diagnostic push
#if defined(__has_warning) && __has_warning("-Wunused-private-field")
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif
#endif
    RenderLayer& m_layer;
#if PLATFORM(IOS)
#pragma clang diagnostic pop
#endif

    RefPtr<FilterEffectRenderer> m_renderer;
    LayoutRect m_dirtySourceRect;

#if ENABLE(SVG)
    Vector<RefPtr<Element>> m_internalSVGReferences;
    Vector<CachedResourceHandle<CachedSVGDocument>> m_externalSVGReferences;
#endif
};

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)

#endif // RenderLayerFilterInfo_h
