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
#include "RenderAncestorIterator.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderSVGViewportContainer.h"
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
        // The mapping into parent coordinate systems stops at this renderer,
        // as mapLocalContainer exits if "ancestorContainer == this" is fulfilled.
        const RenderLayerModelObject* ancestorContainer = nullptr;

        // Special handling of RenderSVGRoot, due to the way we implement the outermost <svg> element.
        // When SVGSVGElement::getCTM()/getScreenCTM() is invoked, we want to use information from the
        // anonymous RenderSVGViewportContainer (most noticeable: viewBox). Therefore we have to start
        // calling mapLocalToContainer() starting from the anonymous RenderSVGViewportContainer, and
        // not from its parent - RenderSVGRoot.
        auto* renderer = &m_renderer;
        if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(renderer)) {
            renderer = svgRoot->viewportContainer();
            if (trackingMode == TransformState::TrackSVGCTMMatrix)
                ancestorContainer = svgRoot;
            else
                ASSERT(!stopAtRenderer);
        } else if (trackingMode == TransformState::TrackSVGCTMMatrix) {
            // Only ever walk up to the anonymous RenderSVGViewportContainer (the first and only child of RenderSVGRoot).
            // Proceeding up to RenderSVGRoot would include border/padding/margin information which shouldn't be included for getCTM() (unlike getScreenCTM()).
            if (!stopAtRenderer) {
                if (auto* svgRoot = ancestorsOfType<RenderSVGRoot>(*renderer).first())
                    ancestorContainer = svgRoot->viewportContainer();
            } else if (const auto* enclosingLayerRenderer = ancestorsOfType<RenderLayerModelObject>(*stopAtRenderer).first())
                ancestorContainer = enclosingLayerRenderer;
        } else if (trackingMode == TransformState::TrackSVGScreenCTMMatrix && stopAtRenderer) {
            ASSERT(stopAtRenderer->isComposited());
            ancestorContainer = ancestorsOfType<RenderLayerModelObject>(*stopAtRenderer).first();
        }

        TransformState transformState(m_renderer.settings().css3DTransformInteroperabilityEnabled(), TransformState::ApplyTransformDirection, FloatPoint { });
        transformState.setTransformMatrixTracking(trackingMode);

        renderer->mapLocalToContainer(ancestorContainer, transformState, { UseTransforms, ApplyContainerFlip });

        if (trackingMode == TransformState::TrackSVGCTMMatrix) {
            if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(m_renderer))
                transformState.move(-toLayoutSize(svgRoot->contentBoxLocation()));
            else if (ancestorContainer) {
                // Continue to accumulate container offsets, excluding transforms, from the container of the current element ('ancestorContainer') up to RenderSVGRoot.
                // The resulting TransformState is aligned with the 'nominalSVGLayoutLocation()' within the local coordinate system of the 'm_renderer'. (0, 0) in local
                // coordinates is mapped to the top-left of the 'objectBoundingBoxWithoutTransforms()' of the SVG renderer.
                if (auto* svgRoot = lineageOfType<RenderSVGRoot>(*ancestorContainer).first())
                    ancestorContainer->mapLocalToContainer(svgRoot->viewportContainer(), transformState, { ApplyContainerFlip });
            }
        }

        transformState.flatten();

        auto transform = transformState.releaseTrackedTransform();
        if (!transform)
            return { };

        auto ctm = transform->toAffineTransform();

        // When we've climbed the ancestor tree up to and including RenderSVGRoot, the CTM is aligned with the top-left of the renderers bounding box (= nominal SVG layout location).
        // However, for getCTM/getScreenCTM we're supposed to align by the top-left corner of the enclosing "viewport element" -- correct for that.
        if (m_renderer.isSVGRoot())
            return ctm;

        ctm.translate(-toFloatSize(m_renderer.nominalSVGLayoutLocation()));
        return ctm;
    }

    float calculateScreenFontSizeScalingFactor() const
    {
        // Walk up the render tree, accumulating transforms
        auto* layer = m_renderer.enclosingLayer();

        RenderLayer* stopAtLayer = nullptr;
        while (layer) {
            // We can stop at compositing layers, to match the backing resolution.
            if (layer->isComposited()) {
                stopAtLayer = layer;
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
