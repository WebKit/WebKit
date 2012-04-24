/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#include "LayoutTypes.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

#if ENABLE(CSS_SHADERS)
#include "CustomFilterProgramClient.h"
#endif

namespace WebCore {

class FilterEffectRenderer;
class FilterOperations;
class RenderLayer;
class RenderLayerFilterInfo;

typedef HashMap<const RenderLayer*, RenderLayerFilterInfo*> RenderLayerFilterInfoMap;
    
class RenderLayerFilterInfo
#if ENABLE(CSS_SHADERS)
    : public CustomFilterProgramClient
#endif
{
public:
    static RenderLayerFilterInfo* filterInfoForRenderLayer(const RenderLayer*);
    static RenderLayerFilterInfo* createFilterInfoForRenderLayerIfNeeded(RenderLayer*);
    static void removeFilterInfoForRenderLayer(RenderLayer*);

    const LayoutRect& dirtySourceRect() const { return m_dirtySourceRect; }
    void expandDirtySourceRect(const LayoutRect& rect) { m_dirtySourceRect.unite(rect); }
    void resetDirtySourceRect() { m_dirtySourceRect = LayoutRect(); }
    
    FilterEffectRenderer* renderer() const { return m_renderer.get(); }
    void setRenderer(PassRefPtr<FilterEffectRenderer>);
    
#if ENABLE(CSS_SHADERS)
    // Implementation of the CustomFilterProgramClient interface.
    virtual void notifyCustomFilterProgramLoaded(CustomFilterProgram*);

    void updateCustomFilterClients(const FilterOperations&);
    void removeCustomFilterClients();
#endif

    
private:
    RenderLayerFilterInfo(RenderLayer*);
    ~RenderLayerFilterInfo();
    
    RenderLayer* m_layer;
    
    RefPtr<FilterEffectRenderer> m_renderer;
    LayoutRect m_dirtySourceRect;
    
#if ENABLE(CSS_SHADERS)
    typedef Vector<RefPtr<CustomFilterProgram> > CustomFilterProgramList;
    CustomFilterProgramList m_cachedCustomFilterPrograms;
#endif
    
    static RenderLayerFilterInfoMap* s_filterMap;
};

} // namespace WebCore

#endif // ENABLE(CSS_FILTERS)

#endif // RenderLayerFilterInfo_h
