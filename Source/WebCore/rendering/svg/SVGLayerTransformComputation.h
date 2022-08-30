/*
 * Copyright (C) 2020, 2021, 2022 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderLayerModelObject.h"
#include "TransformState.h"
#include <wtf/MathExtras.h>

namespace WebCore {

class SVGLayerTransformComputation {
    WTF_MAKE_NONCOPYABLE(SVGLayerTransformComputation);
public:
    SVGLayerTransformComputation(const RenderLayerModelObject& renderer)
        : m_renderer(renderer)
    {
    }

    ~SVGLayerTransformComputation() = default;

    AffineTransform computeAccumulatedTransform(const RenderLayerModelObject* stopAtRenderer, TransformState::TransformMatrixTracking trackingMode) const
    {
        TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint { });
        transformState.setTransformMatrixTracking(trackingMode);

        const RenderLayerModelObject* repaintContainer = nullptr;
        if (stopAtRenderer && stopAtRenderer->parent()) {
            if (auto* enclosingLayer = stopAtRenderer->parent()->enclosingLayer())
                repaintContainer = &enclosingLayer->renderer();
        }

        m_renderer.mapLocalToContainer(repaintContainer, transformState, { UseTransforms, ApplyContainerFlip });
        transformState.flatten();

        if (auto transform = transformState.releaseTrackedTransform())
            return transform->toAffineTransform();

        return { };
    }

    float calculateScreenFontSizeScalingFactor() const
    {
        // Walk up the render tree, accumulating transforms
        auto* layer = m_renderer.enclosingLayer();

        RenderLayer* stopAtLayer = nullptr;
        while (layer) {
            // We can stop at compositing layers, to match the backing resolution.
            if (layer->isComposited()) {
                stopAtLayer = layer->parent();
                break;
            }

            layer = layer->parent();
        }

        auto ctm = computeAccumulatedTransform(stopAtLayer ? &stopAtLayer->renderer() : nullptr, TransformState::TrackSVGScreenCTMMatrix);
        ctm.scale(m_renderer.document().deviceScaleFactor());
        return narrowPrecisionToFloat(std::hypot(ctm.xScale(), ctm.yScale()) / sqrtOfTwoDouble);
    }

private:
    const RenderLayerModelObject& m_renderer;
};

} // namespace WebCore

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
