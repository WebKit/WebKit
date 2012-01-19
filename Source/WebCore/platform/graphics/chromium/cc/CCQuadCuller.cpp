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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "cc/CCQuadCuller.h"

#include "Region.h"
#include "TransformationMatrix.h"
#include "cc/CCLayerImpl.h"
#include "cc/CCRenderPass.h"
#include "cc/CCRenderSurfaceDrawQuad.h"

namespace std {

// Specialize for OwnPtr<CCDrawQuad> since Vector doesn't know how to reverse a Vector of OwnPtr<T> in general.
template<>
void swap(OwnPtr<WebCore::CCDrawQuad>& a, OwnPtr<WebCore::CCDrawQuad>& b)
{
    a.swap(b);
}

}

namespace WebCore {

static bool regionContainsRect(const Region& region, const IntRect& rect)
{
    Region rectRegion(rect);
    Region intersectRegion(intersect(region, rectRegion));

    if (intersectRegion.isEmpty())
        return false;

    rectRegion.subtract(intersectRegion);
    return rectRegion.isEmpty();
}

void CCQuadCuller::cullOccludedQuads(CCQuadList& quadList)
{
    if (!quadList.size())
        return;

    CCQuadList culledList;
    culledList.reserveCapacity(quadList.size());

    Region opaqueCoverageThusFar;

    for (int i = quadList.size() - 1; i >= 0; --i) {
        CCDrawQuad* drawQuad = quadList[i].get();

        IntRect quadRect(drawQuad->quadTransform().mapRect(drawQuad->quadRect()));

        bool keepQuad = !regionContainsRect(opaqueCoverageThusFar, quadRect);

        if (keepQuad && drawQuad->isLayerAxisAlignedIntRect())
            opaqueCoverageThusFar.unite(drawQuad->quadTransform().mapRect(drawQuad->opaqueRect()));

        if (keepQuad)
            culledList.append(quadList[i].release());
    }
    quadList.clear(); // Release anything that remains.

    culledList.reverse();
    quadList.swap(culledList);
}

} // namespace WebCore
#endif // USE(ACCELERATED_COMPOSITING)
