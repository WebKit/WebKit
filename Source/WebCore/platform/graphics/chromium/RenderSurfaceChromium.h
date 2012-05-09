/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef RenderSurfaceChromium_h
#define RenderSurfaceChromium_h

#if USE(ACCELERATED_COMPOSITING)

#include "FloatRect.h"
#include "IntRect.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include "TextureManager.h"
#include "TransformationMatrix.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class FilterOperations;
class LayerChromium;
class LayerRendererChromium;
class ManagedTexture;

class RenderSurfaceChromium {
    WTF_MAKE_NONCOPYABLE(RenderSurfaceChromium);
public:
    explicit RenderSurfaceChromium(LayerChromium*);
    ~RenderSurfaceChromium();

    // Returns the rect that encloses the RenderSurface including any reflection.
    FloatRect drawableContentRect() const;

    const IntRect& contentRect() const { return m_contentRect; }
    void setContentRect(const IntRect& contentRect) { m_contentRect = contentRect; }

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float drawOpacity) { m_drawOpacity = drawOpacity; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    // This goes from content space with the origin in the center of the rect being transformed to the target space with the origin in the top left of the
    // rect being transformed. Position the rect so that the origin is in the center of it before applying this transform.
    const TransformationMatrix& drawTransform() const { return m_drawTransform; }
    void setDrawTransform(const TransformationMatrix& drawTransform) { m_drawTransform = drawTransform; }

    const TransformationMatrix& originTransform() const { return m_originTransform; }
    void setOriginTransform(const TransformationMatrix& originTransform) { m_originTransform = originTransform; }

    const TransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }
    void setScreenSpaceTransform(const TransformationMatrix& screenSpaceTransform) { m_screenSpaceTransform = screenSpaceTransform; }

    const TransformationMatrix& replicaDrawTransform() const { return m_replicaDrawTransform; }
    void setReplicaDrawTransform(const TransformationMatrix& replicaDrawTransform) { m_replicaDrawTransform = replicaDrawTransform; }

    const TransformationMatrix& replicaOriginTransform() const { return m_replicaOriginTransform; }
    void setReplicaOriginTransform(const TransformationMatrix& replicaOriginTransform) { m_replicaOriginTransform = replicaOriginTransform; }

    const TransformationMatrix& replicaScreenSpaceTransform() const { return m_replicaScreenSpaceTransform; }
    void setReplicaScreenSpaceTransform(const TransformationMatrix& replicaScreenSpaceTransform) { m_replicaScreenSpaceTransform = replicaScreenSpaceTransform; }

    bool targetSurfaceTransformsAreAnimating() const { return m_targetSurfaceTransformsAreAnimating; }
    void setTargetSurfaceTransformsAreAnimating(bool animating) { m_targetSurfaceTransformsAreAnimating = animating; }
    bool screenSpaceTransformsAreAnimating() const { return m_screenSpaceTransformsAreAnimating; }
    void setScreenSpaceTransformsAreAnimating(bool animating) { m_screenSpaceTransformsAreAnimating = animating; }

    const IntRect& clipRect() const { return m_clipRect; }
    void setClipRect(const IntRect& clipRect) { m_clipRect = clipRect; }

    void setFilters(const FilterOperations& filters) { m_filters = filters; }
    const FilterOperations& filters() const { return m_filters; }

    void setBackgroundFilters(const FilterOperations& filters) { m_backgroundFilters = filters; }
    const FilterOperations& backgroundFilters() const { return m_backgroundFilters; }

    bool skipsDraw() const { return m_skipsDraw; }
    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }

    void clearLayerList() { m_layerList.clear(); }
    Vector<RefPtr<LayerChromium> >& layerList() { return m_layerList; }

    void setMaskLayer(LayerChromium* maskLayer) { m_maskLayer = maskLayer; }

    void setNearestAncestorThatMovesPixels(RenderSurfaceChromium* surface) { m_nearestAncestorThatMovesPixels = surface; }
    const RenderSurfaceChromium* nearestAncestorThatMovesPixels() const { return m_nearestAncestorThatMovesPixels; }

    bool hasReplica() const;

    bool hasMask() const;
    bool replicaHasMask() const;

private:
    LayerChromium* m_owningLayer;
    LayerChromium* m_maskLayer;

    IntRect m_contentRect;
    bool m_skipsDraw;

    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;
    TransformationMatrix m_drawTransform;
    TransformationMatrix m_originTransform;
    TransformationMatrix m_screenSpaceTransform;
    TransformationMatrix m_replicaDrawTransform;
    TransformationMatrix m_replicaOriginTransform;
    TransformationMatrix m_replicaScreenSpaceTransform;
    bool m_targetSurfaceTransformsAreAnimating;
    bool m_screenSpaceTransformsAreAnimating;
    FilterOperations m_filters;
    FilterOperations m_backgroundFilters;
    IntRect m_clipRect;
    Vector<RefPtr<LayerChromium> > m_layerList;

    // The nearest ancestor target surface that will contain the contents of this surface, and that is going
    // to move pixels within the surface (such as with a blur). This can point to itself.
    RenderSurfaceChromium* m_nearestAncestorThatMovesPixels;

    // For CCLayerIteratorActions
    int m_targetRenderSurfaceLayerIndexHistory;
    int m_currentLayerIndexHistory;
    friend struct CCLayerIteratorActions;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
