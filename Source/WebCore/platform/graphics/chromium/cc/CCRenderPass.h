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

#ifndef CCRenderPass_h
#define CCRenderPass_h

#include "cc/CCDrawQuad.h"
#include "cc/CCOcclusionTracker.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerImpl;
class CCRenderSurface;
class CCSharedQuadState;
class Color;

// A list of CCDrawQuad objects, sorted internally in front-to-back order.
class CCQuadList : public Vector<OwnPtr<CCDrawQuad> > {
public:
    typedef reverse_iterator backToFrontIterator;
    typedef const_reverse_iterator constBackToFrontIterator;

    inline backToFrontIterator backToFrontBegin() { return rbegin(); }
    inline backToFrontIterator backToFrontEnd() { return rend(); }
    inline constBackToFrontIterator backToFrontBegin() const { return rbegin(); }
    inline constBackToFrontIterator backToFrontEnd() const { return rend(); }
};

class CCRenderPass {
    WTF_MAKE_NONCOPYABLE(CCRenderPass);
public:
    static PassOwnPtr<CCRenderPass> create(CCRenderSurface*);

    void appendQuadsForLayer(CCLayerImpl*, CCOcclusionTrackerImpl*, bool& hadMissingTiles);
    void appendQuadsForRenderSurfaceLayer(CCLayerImpl*, CCOcclusionTrackerImpl*);
    void appendQuadsToFillScreen(CCLayerImpl* rootLayer, const Color& screenBackgroundColor, const CCOcclusionTrackerImpl&);

    const CCQuadList& quadList() const { return m_quadList; }
    CCRenderSurface* targetSurface() const { return m_targetSurface; }

    void setSurfaceDamageRect(const FloatRect& surfaceDamageRect) { m_surfaceDamageRect = surfaceDamageRect; }
    const FloatRect& surfaceDamageRect() const { return m_surfaceDamageRect; }

private:
    explicit CCRenderPass(CCRenderSurface*);

    CCRenderSurface* m_targetSurface;
    CCQuadList m_quadList;
    Vector<OwnPtr<CCSharedQuadState> > m_sharedQuadStateList;
    FloatRect m_surfaceDamageRect;
};

typedef Vector<OwnPtr<CCRenderPass> > CCRenderPassList;

}

#endif
