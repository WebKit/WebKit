/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "LayoutRect.h"
#include "RenderGeometryMap.h"

namespace WTF {
class TextStream;
}

namespace WebCore {

class OverflowAwareOverlapContainer;
class OverlapMapContainer;
class RenderLayer;

class LayerOverlapMap {
    WTF_MAKE_NONCOPYABLE(LayerOverlapMap);
public:
    LayerOverlapMap(const RenderLayer& rootLayer);
    ~LayerOverlapMap();
    
    struct LayerAndBounds {
        RenderLayer& layer;
        LayoutRect bounds;
    };

    void add(const RenderLayer&, const LayoutRect&, const Vector<LayerAndBounds>& enclosingClippingLayers);
    bool overlapsLayers(const RenderLayer&, const LayoutRect&, const Vector<LayerAndBounds>& enclosingClippingLayers) const;
    bool isEmpty() const { return m_isEmpty; }

    void pushCompositingContainer(const RenderLayer&);
    void popCompositingContainer(const RenderLayer&);

    void pushSpeculativeCompositingContainer(const RenderLayer&);
    void confirmSpeculativeCompositingContainer();
    bool maybePopSpeculativeCompositingContainer();

    const RenderGeometryMap& geometryMap() const { return m_geometryMap; }
    RenderGeometryMap& geometryMap() { return m_geometryMap; }

    const Vector<std::unique_ptr<OverlapMapContainer>>& overlapStack() const { return m_overlapStack; }

private:
    Vector<std::unique_ptr<OverlapMapContainer>> m_overlapStack;
    Vector<std::unique_ptr<OverlapMapContainer>> m_speculativeOverlapStack;
    RenderGeometryMap m_geometryMap;
    const RenderLayer& m_rootLayer;
    bool m_isEmpty { true };
};

TextStream& operator<<(TextStream&, const LayerOverlapMap&);

} // namespace WebCore
