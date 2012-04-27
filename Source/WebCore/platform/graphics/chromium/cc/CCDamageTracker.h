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

#ifndef CCDamageTracker_h
#define CCDamageTracker_h

#include "FloatRect.h"
#include <wtf/HashMap.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerImpl;
class CCRenderSurface;
class FilterOperations;

// Computes the region where pixels have actually changed on a RenderSurface. This region is used
// to scissor what is actually drawn to the screen to save GPU computation and bandwidth.
class CCDamageTracker {
public:
    static PassOwnPtr<CCDamageTracker> create();
    ~CCDamageTracker();

    void forceFullDamageNextUpdate() { m_forceFullDamageNextUpdate = true; }
    void updateDamageTrackingState(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID, bool targetSurfacePropertyChangedOnlyFromDescendant, const IntRect& targetSurfaceContentRect, CCLayerImpl* targetSurfaceMaskLayer, const FilterOperations&);
    const FloatRect& currentDamageRect() { return m_currentDamageRect; }

private:
    CCDamageTracker();

    FloatRect trackDamageFromActiveLayers(const Vector<CCLayerImpl*>& layerList, int targetSurfaceLayerID);
    FloatRect trackDamageFromSurfaceMask(CCLayerImpl* targetSurfaceMaskLayer);
    FloatRect trackDamageFromLeftoverRects();

    FloatRect removeRectFromCurrentFrame(int layerID, bool& layerIsNew);
    void saveRectForNextFrame(int layerID, const FloatRect& targetSpaceRect);

    // These helper functions are used only in trackDamageFromActiveLayers().
    void extendDamageForLayer(CCLayerImpl*, FloatRect& targetDamageRect);
    void extendDamageForRenderSurface(CCLayerImpl*, FloatRect& targetDamageRect);

    // To correctly track exposed regions, two hashtables of rects are maintained.
    // The "current" map is used to compute exposed regions of the current frame, while
    // the "next" map is used to collect layer rects that are used in the next frame.
    typedef HashMap<int, FloatRect> RectMap;
    OwnPtr<RectMap> m_currentRectHistory;
    OwnPtr<RectMap> m_nextRectHistory;

    FloatRect m_currentDamageRect;
    bool m_forceFullDamageNextUpdate;
};

} // namespace WebCore

#endif // CCDamageTracker_h
