/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef CCQuadCuller_h
#define CCQuadCuller_h

#include "CCQuadSink.h"
#include "CCRenderPass.h"

namespace WebCore {
class CCLayerImpl;
class CCRenderSurface;
template<typename LayerType, typename SurfaceType>
class CCOcclusionTrackerBase;

class CCQuadCuller : public CCQuadSink {
public:
    CCQuadCuller(CCQuadList&, CCSharedQuadStateList&, CCLayerImpl*, const CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>*, bool showCullingWithDebugBorderQuads, bool forSurface);
    virtual ~CCQuadCuller() { }

    // CCQuadSink implementation.
    virtual CCSharedQuadState* useSharedQuadState(PassOwnPtr<CCSharedQuadState>) OVERRIDE;
    virtual bool append(PassOwnPtr<CCDrawQuad>, CCAppendQuadsData&) OVERRIDE;

private:
    CCQuadList& m_quadList;
    CCSharedQuadStateList& m_sharedQuadStateList;
    CCSharedQuadState* m_currentSharedQuadState;
    CCLayerImpl* m_layer;
    const CCOcclusionTrackerBase<CCLayerImpl, CCRenderSurface>* m_occlusionTracker;
    bool m_showCullingWithDebugBorderQuads;
    bool m_forSurface;
};

}
#endif // CCQuadCuller_h
