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

#ifndef CCDrawQuad_h
#define CCDrawQuad_h

#include "cc/CCSharedQuadState.h"

namespace WebCore {

class CCDebugBorderDrawQuad;
class CCRenderSurfaceDrawQuad;
class CCSolidColorDrawQuad;
class CCTileDrawQuad;
class CCCanvasDrawQuad;
class CCVideoDrawQuad;
class CCPluginDrawQuad;

// CCDrawQuad is a bag of data used for drawing a quad. Because different
// materials need different bits of per-quad data to render, classes that derive
// from CCDrawQuad store additional data in their derived instance. The Material
// enum is used to "safely" upcast to the derived class.
class CCDrawQuad {
    WTF_MAKE_NONCOPYABLE(CCDrawQuad);
public:
    const IntRect& quadRect() const { return m_quadRect; }
    const TransformationMatrix& quadTransform() const { return m_sharedQuadState->quadTransform(); }
    const TransformationMatrix& layerTransform() const { return m_sharedQuadState->layerTransform(); }
    const IntRect& layerRect() const { return m_sharedQuadState->layerRect(); }
    const IntRect& clipRect() const { return m_sharedQuadState->clipRect(); }
    float opacity() const { return m_sharedQuadState->opacity(); }
    // For the purposes of culling, are the contents of this quad opaque?
    bool drawsOpaque() const { return m_sharedQuadState->isOpaque() && m_quadOpaque && opacity() == 1; }
    bool needsBlending() const { return !m_sharedQuadState->isOpaque() || m_needsBlending || opacity() != 1; }
    bool isLayerAxisAlignedIntRect() const { return m_sharedQuadState->isLayerAxisAlignedIntRect(); }

    // Allows changing the rect that gets drawn to make it smaller. Parameter passed
    // in will be clipped to quadRect().
    void setQuadVisibleRect(const IntRect&);
    const IntRect& quadVisibleRect() const { return m_quadVisibleRect; }

    enum Material {
        Invalid,
        DebugBorder,
        RenderSurface,
        SolidColor,
        TiledContent,
        CanvasContent,
        VideoContent,
        PluginContent,
    };

    Material material() const { return m_material; }

    const CCDebugBorderDrawQuad* toDebugBorderDrawQuad() const;
    const CCRenderSurfaceDrawQuad* toRenderSurfaceDrawQuad() const;
    const CCSolidColorDrawQuad* toSolidColorDrawQuad() const;
    const CCTileDrawQuad* toTileDrawQuad() const;
    const CCCanvasDrawQuad* toCanvasDrawQuad() const;
    const CCVideoDrawQuad* toVideoDrawQuad() const;
    const CCPluginDrawQuad* toPluginDrawQuad() const;

protected:
    CCDrawQuad(const CCSharedQuadState*, Material, const IntRect&);

    const CCSharedQuadState* m_sharedQuadState;

    Material m_material;
    IntRect m_quadRect;
    IntRect m_quadVisibleRect;

    // By default, the shared quad state determines whether or not this quad is
    // opaque or needs blending. Derived classes can override with these
    // variables.
    bool m_quadOpaque;
    bool m_needsBlending;
};

}

#endif
