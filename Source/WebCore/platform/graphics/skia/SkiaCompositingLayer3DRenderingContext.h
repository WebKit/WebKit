/*
 * Copyright (C) 2024 Jani Hautakangas <jani@kodegood.com>
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(COORDINATED_GRAPHICS) && USE(SKIA)
#include "FloatPolygon3D.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkPath.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class FloatPlane3D;
class SkiaCompositingLayer;

class SkiaCompositingLayer3DRenderingContext {
public:
    static void paint(const Vector<Ref<SkiaCompositingLayer>>&,
        const std::function<void(SkiaCompositingLayer&, std::optional<SkPath>)>&);

private:
    enum class LayerPosition {
        InFront,
        Behind,
        Coplanar,
        Intersecting
    };

    struct BoundingBox {
        FloatPoint3D min;
        FloatPoint3D max;
    };

    struct Layer {
        FloatPolygon3D geometry;
        BoundingBox boundingBox;
        Ref<SkiaCompositingLayer> compositingLayer;
        bool isSplit { false };
    };

    struct LayerNode {
        WTF_MAKE_STRUCT_TZONE_ALLOCATED(LayerNode);

        explicit LayerNode(Layer&& layer)
        {
            layers.append(WTF::move(layer));
        }

        const Layer& firstLayer() const { return layers[0]; }

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

#endif // USE(COORDINATED_GRAPHICS) && USE(SKIA)
