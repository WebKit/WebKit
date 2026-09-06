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

#include "RenderAncestorIterator.h"
#include "RenderLayer.h"
#include "RenderLayerModelObject.h"
#include "RenderSVGViewportContainer.h"
#include "TransformState.h"
#include <wtf/MathExtras.h>

namespace WebCore {

enum class StopAtRendererTransform : bool { Exclude, Include };

class SVGTransformComputation {
    WTF_MAKE_NONCOPYABLE(SVGTransformComputation);
public:
    SVGTransformComputation(const RenderLayerModelObject& renderer)
        : m_renderer(renderer)
    {
    }

    ~SVGTransformComputation() = default;

    AffineTransform computeAccumulatedTransform(const RenderLayerModelObject* stopAtRenderer, TransformState::TransformMatrixTracking trackingMode, StopAtRendererTransform stopAtRendererTransform = StopAtRendererTransform::Include) const
    {
        // The mapping into parent coordinate systems stops at this renderer,
        // as mapLocalContainer exits if "ancestorContainer == this" is fulfilled.
        const RenderLayerModelObject* ancestorContainer = nullptr;

        // Special handling of RenderSVGRoot, due to the way we implement the outermost <svg> element.
        // When SVGSVGElement::getCTM()/getScreenCTM() is invoked, we want to use information from the
        // anonymous RenderSVGViewportContainer (most noticeable: viewBox). Therefore we have to start
        // calling mapLocalToContainer() starting from the anonymous RenderSVGViewportContainer, and
        // not from its parent - RenderSVGRoot.
        auto* renderer = m_renderer.ptr();
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
            } else if (stopAtRendererTransform == StopAtRendererTransform::Include) {
                if (const auto* enclosingLayerRenderer = ancestorsOfType<RenderLayerModelObject>(*stopAtRenderer).first())
                    ancestorContainer = enclosingLayerRenderer;
            } else
                ancestorContainer = stopAtRenderer;
        } else if (trackingMode == TransformState::TrackSVGScreenCTMMatrix && stopAtRenderer)
            ancestorContainer = ancestorsOfType<RenderLayerModelObject>(*stopAtRenderer).first();

        TransformState transformState(TransformState::ApplyTransformDirection, FloatPoint { });
        transformState.setTransformMatrixTracking(trackingMode);

        renderer->mapLocalToContainer(ancestorContainer, transformState, { MapCoordinatesMode::UseTransforms, MapCoordinatesMode::ApplyContainerFlip });

        if (trackingMode == TransformState::TrackSVGCTMMatrix) {
            if (auto* svgRoot = dynamicDowncast<RenderSVGRoot>(m_renderer.get()))
                transformState.move(-toLayoutSize(svgRoot->contentBoxLocation()));
            else if (ancestorContainer) {
                // Continue to accumulate container offsets, excluding transforms, from the container of the current element ('ancestorContainer') up to RenderSVGRoot.
                // The resulting TransformState is aligned with the 'nominalSVGLayoutLocation()' within the local coordinate system of the 'm_renderer'. (0, 0) in local
                // coordinates is mapped to the top-left of the 'objectBoundingBoxWithoutTransforms()' of the SVG renderer.
                if (auto* svgRoot = lineageOfType<RenderSVGRoot>(*ancestorContainer).first())
                    ancestorContainer->mapLocalToContainer(svgRoot->viewportContainer(), transformState, { MapCoordinatesMode::ApplyContainerFlip });
            }
        }

        auto transform = transformState.releaseTrackedTransform();
        if (!transform)
            return { };

        auto ctm = transform->toAffineTransform();

        // When we've climbed the ancestor tree up to and including RenderSVGRoot, the CTM is aligned with the top-left of the renderers bounding box (= nominal SVG layout location).
        // However, for getCTM/getScreenCTM we're supposed to align by the top-left corner of the enclosing "viewport element" -- correct for that.
        if (m_renderer->isRenderSVGRoot())
            return ctm;

        ctm.translate(-toFloatSize(m_renderer->nominalSVGLayoutLocation()));
        return ctm;
    }

    AffineTransform computeTransformToSVGRoot() const
    {
        auto* svgRoot = ancestorsOfType<RenderSVGRoot>(m_renderer.get()).first();
        if (!svgRoot)
            return { };

        // Accumulate up to and including the anonymous RenderSVGViewportContainer, which carries the 'viewBox'
        // transformation (and page zoom) -- but stop before the transform of 'svgRoot' itself.
        auto* viewportContainer = svgRoot->viewportContainer();
        if (!viewportContainer)
            return { };

        auto ctm = computeAccumulatedTransform(viewportContainer, TransformState::TrackSVGScreenCTMMatrix);
        auto zoom = svgRoot->style().usedZoom();
        if (zoom == 1)
            return ctm;

        return AffineTransform::makeScale({ 1 / zoom, 1 / zoom }).multiply(ctm);
    }

    FloatSize calculateAccumulatedSVGAncestorTransformScale()
    {
        AffineTransform accumulatedTransform = computeAccumulatedTransform(nullptr, TransformState::TrackSVGScreenCTMMatrix);
        return {
            narrowPrecisionToFloat(accumulatedTransform.xScale()),
            narrowPrecisionToFloat(accumulatedTransform.yScale())
        };
    }

private:
    SingleThreadWeakRef<const RenderLayerModelObject> m_renderer;
};

} // namespace WebCore
