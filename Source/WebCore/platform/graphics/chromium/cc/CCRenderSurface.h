/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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


#ifndef CCRenderSurface_h
#define CCRenderSurface_h

#if USE(ACCELERATED_COMPOSITING)

#include "FilterOperations.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "ProgramBinding.h"
#include "ShaderChromium.h"
#include "SkBitmap.h"
#include "TextureManager.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerQuad.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class CCDamageTracker;
class CCSharedQuadState;
class CCLayerImpl;
class LayerRendererChromium;
class ManagedTexture;

class CCRenderSurface {
    WTF_MAKE_NONCOPYABLE(CCRenderSurface);
public:
    explicit CCRenderSurface(CCLayerImpl*);
    ~CCRenderSurface();

    bool prepareContentsTexture(LayerRendererChromium*);
    void releaseContentsTexture();

    void setScissorRect(LayerRendererChromium*, const FloatRect& surfaceDamageRect) const;

    void drawContents(LayerRendererChromium*);
    void drawReplica(LayerRendererChromium*);

    String name() const;
    void dumpSurface(TextStream&, int indent) const;

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }

    // Returns the rect that encloses the RenderSurface including any reflection.
    FloatRect drawableContentRect() const;

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    void setFilters(const FilterOperations& filters) { m_filters = filters; }
    const FilterOperations& filters() const { return m_filters; }
    SkBitmap applyFilters(LayerRendererChromium*);

    void setNearestAncestorThatMovesPixels(CCRenderSurface* surface) { m_nearestAncestorThatMovesPixels = surface; }
    const CCRenderSurface* nearestAncestorThatMovesPixels() const { return m_nearestAncestorThatMovesPixels; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    void setDrawTransform(const TransformationMatrix& drawTransform) { m_drawTransform = drawTransform; }
    const TransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setOriginTransform(const TransformationMatrix& originTransform) { m_originTransform = originTransform; }
    const TransformationMatrix& originTransform() const { return m_originTransform; }

    void setScreenSpaceTransform(const TransformationMatrix& screenSpaceTransform) { m_screenSpaceTransform = screenSpaceTransform; }
    const TransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }

    void setReplicaDrawTransform(const TransformationMatrix& replicaDrawTransform) { m_replicaDrawTransform = replicaDrawTransform; }
    const TransformationMatrix& replicaDrawTransform() const { return m_replicaDrawTransform; }

    void setReplicaOriginTransform(const TransformationMatrix& replicaOriginTransform) { m_replicaOriginTransform = replicaOriginTransform; }
    const TransformationMatrix& replicaOriginTransform() const { return m_replicaOriginTransform; }

    void setReplicaScreenSpaceTransform(const TransformationMatrix& replicaScreenSpaceTransform) { m_replicaScreenSpaceTransform = replicaScreenSpaceTransform; }
    const TransformationMatrix& replicaScreenSpaceTransform() const { return m_replicaScreenSpaceTransform; }

    bool targetSurfaceTransformsAreAnimating() const { return m_targetSurfaceTransformsAreAnimating; }
    void setTargetSurfaceTransformsAreAnimating(bool animating) { m_targetSurfaceTransformsAreAnimating = animating; }
    bool screenSpaceTransformsAreAnimating() const { return m_screenSpaceTransformsAreAnimating; }
    void setScreenSpaceTransformsAreAnimating(bool animating) { m_screenSpaceTransformsAreAnimating = animating; }

    // Usage: this clipRect should not be used if one of the two conditions is true: (a) clipRect() is empty, or (b) owningLayer->parent()->usesLayerClipping() is false.
    void setClipRect(const IntRect&);
    const IntRect& clipRect() const { return m_clipRect; }

    void setContentRect(const IntRect&);
    const IntRect& contentRect() const { return m_contentRect; }

    void setSkipsDraw(bool skipsDraw) { m_skipsDraw = skipsDraw; }
    bool skipsDraw() const { return m_skipsDraw; }

    void clearLayerList() { m_layerList.clear(); }
    Vector<CCLayerImpl*>& layerList() { return m_layerList; }

    void setMaskLayer(CCLayerImpl* maskLayer) { m_maskLayer = maskLayer; }

    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlpha> Program;
    typedef ProgramBinding<VertexShaderPosTex, FragmentShaderRGBATexAlphaMask> MaskProgram;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaAA> ProgramAA;
    typedef ProgramBinding<VertexShaderQuad, FragmentShaderRGBATexAlphaMaskAA> MaskProgramAA;

    ManagedTexture* contentsTexture() const { return m_contentsTexture.get(); }

    int owningLayerId() const;

    bool hasReplica() const;

    void resetPropertyChangedFlag() { m_surfacePropertyChanged = false; }
    bool surfacePropertyChanged() const;
    bool surfacePropertyChangedOnlyFromDescendant() const;

    CCDamageTracker* damageTracker() const { return m_damageTracker.get(); }

    PassOwnPtr<CCSharedQuadState> createSharedQuadState() const;
    PassOwnPtr<CCSharedQuadState> createReplicaSharedQuadState() const;

private:
    void drawLayer(LayerRendererChromium*, CCLayerImpl*, const TransformationMatrix&, const SkBitmap& filterBitmap);
    template <class T>
    void drawSurface(LayerRendererChromium*, CCLayerImpl*, const TransformationMatrix& drawTransform, const TransformationMatrix& deviceTransform, const CCLayerQuad& deviceRect, const CCLayerQuad&, const T* program, int shaderMaskSamplerLocation, int shaderQuadLocation, int shaderEdgeLocation, const SkBitmap& filterBitmap);

    CCLayerImpl* m_owningLayer;
    CCLayerImpl* m_maskLayer;

    IntRect m_contentRect;
    bool m_skipsDraw;
    bool m_surfacePropertyChanged;

    OwnPtr<ManagedTexture> m_contentsTexture;
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
    IntRect m_clipRect;
    Vector<CCLayerImpl*> m_layerList;

    // The nearest ancestor target surface that will contain the contents of this surface, and that is going
    // to move pixels within the surface (such as with a blur). This can point to itself.
    CCRenderSurface* m_nearestAncestorThatMovesPixels;

    OwnPtr<CCDamageTracker> m_damageTracker;

    // Stored in the "surface space" where this damage can be used for scissoring.
    FloatRect m_damageRect;

    // For CCLayerIteratorActions
    int m_targetRenderSurfaceLayerIndexHistory;
    int m_currentLayerIndexHistory;
    friend struct CCLayerIteratorActions;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
