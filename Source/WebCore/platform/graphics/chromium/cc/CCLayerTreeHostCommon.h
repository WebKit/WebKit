/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CCLayerTreeHostCommon_h
#define CCLayerTreeHostCommon_h

#include "IntRect.h"
#include "IntSize.h"
#include "TransformationMatrix.h"
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class CCLayerImpl;
class CCLayerSorter;
class LayerChromium;

class CCLayerTreeHostCommon {
public:
    static IntRect calculateVisibleRect(const IntRect& targetSurfaceRect, const IntRect& layerBoundRect, const TransformationMatrix&);
    template<typename LayerType> static IntRect calculateVisibleLayerRect(LayerType*);

    static void calculateDrawTransformsAndVisibility(LayerChromium*, LayerChromium* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<LayerChromium> >& renderSurfaceLayerList, Vector<RefPtr<LayerChromium> >& layerList, int maxTextureSize);
    static void calculateDrawTransformsAndVisibility(CCLayerImpl*, CCLayerImpl* rootLayer, const TransformationMatrix& parentMatrix, const TransformationMatrix& fullHierarchyMatrix, Vector<RefPtr<CCLayerImpl> >& renderSurfaceLayerList, Vector<RefPtr<CCLayerImpl> >& layerList, CCLayerSorter*, int maxTextureSize);

    struct ScrollUpdateInfo {
        int layerId;
        IntSize scrollDelta;
    };
};

typedef Vector<CCLayerTreeHostCommon::ScrollUpdateInfo> CCScrollUpdateSet;

template<typename LayerType>
IntRect CCLayerTreeHostCommon::calculateVisibleLayerRect(LayerType* layer)
{
    ASSERT(layer->targetRenderSurface());
    IntRect targetSurfaceRect = layer->targetRenderSurface()->contentRect();

    if (layer->usesLayerScissor())
        targetSurfaceRect.intersect(layer->scissorRect());

    if (targetSurfaceRect.isEmpty() || layer->contentBounds().isEmpty())
        return targetSurfaceRect;

    // Note carefully these are aliases
    const IntSize& bounds = layer->bounds();
    const IntSize& contentBounds = layer->contentBounds();

    const IntRect layerBoundRect = IntRect(IntPoint(), contentBounds);
    TransformationMatrix transform = layer->drawTransform();

    transform.scaleNonUniform(bounds.width() / static_cast<double>(contentBounds.width()),
                              bounds.height() / static_cast<double>(contentBounds.height()));
    transform.translate(-contentBounds.width() / 2.0, -contentBounds.height() / 2.0);

    IntRect visibleLayerRect = CCLayerTreeHostCommon::calculateVisibleRect(targetSurfaceRect, layerBoundRect, transform);
    return visibleLayerRect;
}

} // namespace WebCore

#endif
