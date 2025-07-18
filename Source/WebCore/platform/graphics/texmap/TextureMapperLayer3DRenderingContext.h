/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "FloatPolygon3D.h"
#include "FloatQuad.h"
#include "TextureMapperLayer.h"
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ClipPath;
class FloatPlane3D;
class TextureMapper;

class TextureMapperLayer3DRenderingContext final {
    WTF_MAKE_TZONE_ALLOCATED(TextureMapperLayerPreserves3DContext);
public:
    void paint(TextureMapper&, const Vector<TextureMapperLayer*>&,
        const std::function<void(TextureMapperLayer*, const ClipPath&)>&);

private:
    enum class LayerPosition {
        InFront,
        Behind,
        Coplanar,
        Intersecting
    };

    struct BoundingBox final {
        FloatPoint3D min;
        FloatPoint3D max;
    };

    struct Layer final {
        FloatPolygon3D geometry;
        BoundingBox boundingBox;
        TextureMapperLayer* textureMapperLayer { nullptr };
        bool isSplitted { false };
        unsigned clipVertexBufferOffset { 0 };
    };

    struct LayerNode final {
        WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(LayerNode);

        explicit LayerNode(Layer&& layer)
        {
            layers.append(WTFMove(layer));
        }

        const Layer& firstLayer() const  { return layers[0]; }

        Vector<Layer> layers;
        std::unique_ptr<LayerNode> frontNode;
        std::unique_ptr<LayerNode> backNode;
    };

    using SweepAndPrunePairs = HashSet<std::pair<size_t, size_t>>;

    static BoundingBox computeBoundingBox(const FloatPolygon3D&);
    static SweepAndPrunePairs sweepAndPrune(const Vector<Layer>&);
    static void buildTree(LayerNode&, Deque<Layer>&);
    static void traverseTree(LayerNode&, const std::function<void(LayerNode&)>&);
    static LayerPosition classifyLayer(const Layer&, const FloatPlane3D&);
};

} // namespace WebCore
