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

#include "FloatRect.h"
#include "IntRect.h"
#include "cc/CCSharedQuadState.h"
#include <public/WebTransformationMatrix.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CCDamageTracker;
class CCQuadSink;
class CCRenderPass;
class CCLayerImpl;
class LayerRendererChromium;
class TextStream;

class CCRenderSurface {
    WTF_MAKE_NONCOPYABLE(CCRenderSurface);
public:
    explicit CCRenderSurface(CCLayerImpl*);
    virtual ~CCRenderSurface();

    String name() const;
    void dumpSurface(TextStream&, int indent) const;

    FloatPoint contentRectCenter() const { return FloatRect(m_contentRect).center(); }

    // Returns the rect that encloses the RenderSurface including any reflection.
    FloatRect drawableContentRect() const;

    float drawOpacity() const { return m_drawOpacity; }
    void setDrawOpacity(float opacity) { m_drawOpacity = opacity; }

    void setNearestAncestorThatMovesPixels(CCRenderSurface* surface) { m_nearestAncestorThatMovesPixels = surface; }
    const CCRenderSurface* nearestAncestorThatMovesPixels() const { return m_nearestAncestorThatMovesPixels; }

    bool drawOpacityIsAnimating() const { return m_drawOpacityIsAnimating; }
    void setDrawOpacityIsAnimating(bool drawOpacityIsAnimating) { m_drawOpacityIsAnimating = drawOpacityIsAnimating; }

    void setDrawTransform(const WebKit::WebTransformationMatrix& drawTransform) { m_drawTransform = drawTransform; }
    const WebKit::WebTransformationMatrix& drawTransform() const { return m_drawTransform; }

    void setScreenSpaceTransform(const WebKit::WebTransformationMatrix& screenSpaceTransform) { m_screenSpaceTransform = screenSpaceTransform; }
    const WebKit::WebTransformationMatrix& screenSpaceTransform() const { return m_screenSpaceTransform; }

    void setReplicaDrawTransform(const WebKit::WebTransformationMatrix& replicaDrawTransform) { m_replicaDrawTransform = replicaDrawTransform; }
    const WebKit::WebTransformationMatrix& replicaDrawTransform() const { return m_replicaDrawTransform; }

    void setReplicaScreenSpaceTransform(const WebKit::WebTransformationMatrix& replicaScreenSpaceTransform) { m_replicaScreenSpaceTransform = replicaScreenSpaceTransform; }
    const WebKit::WebTransformationMatrix& replicaScreenSpaceTransform() const { return m_replicaScreenSpaceTransform; }

    bool targetSurfaceTransformsAreAnimating() const { return m_targetSurfaceTransformsAreAnimating; }
    void setTargetSurfaceTransformsAreAnimating(bool animating) { m_targetSurfaceTransformsAreAnimating = animating; }
    bool screenSpaceTransformsAreAnimating() const { return m_screenSpaceTransformsAreAnimating; }
    void setScreenSpaceTransformsAreAnimating(bool animating) { m_screenSpaceTransformsAreAnimating = animating; }

    void setClipRect(const IntRect&);
    const IntRect& clipRect() const { return m_clipRect; }

    bool contentsChanged() const;

    void setContentRect(const IntRect&);
    const IntRect& contentRect() const { return m_contentRect; }

    void clearLayerList() { m_layerList.clear(); }
    Vector<CCLayerImpl*>& layerList() { return m_layerList; }

    int owningLayerId() const;

    void resetPropertyChangedFlag() { m_surfacePropertyChanged = false; }
    bool surfacePropertyChanged() const;
    bool surfacePropertyChangedOnlyFromDescendant() const;

    CCDamageTracker* damageTracker() const { return m_damageTracker.get(); }

    PassOwnPtr<CCSharedQuadState> createSharedQuadState(int id) const;
    PassOwnPtr<CCSharedQuadState> createReplicaSharedQuadState(int id) const;

    void appendQuads(CCQuadSink&, CCSharedQuadState*, bool forReplica, int renderPassId);

private:
    CCLayerImpl* m_owningLayer;

    // Uses this surface's space.
    IntRect m_contentRect;
    bool m_surfacePropertyChanged;

    float m_drawOpacity;
    bool m_drawOpacityIsAnimating;
    WebKit::WebTransformationMatrix m_drawTransform;
    WebKit::WebTransformationMatrix m_screenSpaceTransform;
    WebKit::WebTransformationMatrix m_replicaDrawTransform;
    WebKit::WebTransformationMatrix m_replicaScreenSpaceTransform;
    bool m_targetSurfaceTransformsAreAnimating;
    bool m_screenSpaceTransformsAreAnimating;

    // Uses the space of the surface's target surface.
    IntRect m_clipRect;

    Vector<CCLayerImpl*> m_layerList;

    // The nearest ancestor target surface that will contain the contents of this surface, and that is going
    // to move pixels within the surface (such as with a blur). This can point to itself.
    CCRenderSurface* m_nearestAncestorThatMovesPixels;

    OwnPtr<CCDamageTracker> m_damageTracker;

    // For CCLayerIteratorActions
    int m_targetRenderSurfaceLayerIndexHistory;
    int m_currentLayerIndexHistory;

    friend struct CCLayerIteratorActions;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
