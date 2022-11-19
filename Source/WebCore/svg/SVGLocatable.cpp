/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "config.h"
#include "SVGLocatable.h"

#include "RenderElement.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGraphicsElement.h"
#include "SVGImageElement.h"
#include "SVGLayerTransformComputation.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "TransformState.h"

namespace WebCore {

static bool isViewportElement(Node* node)
{
    return (node->hasTagName(SVGNames::svgTag)
        || node->hasTagName(SVGNames::symbolTag)
        || node->hasTagName(SVGNames::foreignObjectTag)
        || is<SVGImageElement>(*node));
}

SVGElement* SVGLocatable::nearestViewportElement(const SVGElement* element)
{
    ASSERT(element);
    for (Element* current = element->parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(current))
            return downcast<SVGElement>(current);
    }

    return nullptr;
}

SVGElement* SVGLocatable::farthestViewportElement(const SVGElement* element)
{
    ASSERT(element);
    SVGElement* farthest = nullptr;
    for (Element* current = element->parentOrShadowHostElement(); current; current = current->parentOrShadowHostElement()) {
        if (isViewportElement(current))
            farthest = downcast<SVGElement>(current);
    }
    return farthest;
}

FloatRect SVGLocatable::getBBox(SVGElement* element, StyleUpdateStrategy styleUpdateStrategy)
{
    ASSERT(element);
    if (styleUpdateStrategy == AllowStyleUpdate)
        element->document().updateLayoutIgnorePendingStylesheets();

    // FIXME: Eventually we should support getBBox for detached elements.
    if (!element->renderer())
        return FloatRect();

    return element->renderer()->objectBoundingBox();
}

AffineTransform SVGLocatable::computeCTM(SVGElement* element, CTMScope mode, StyleUpdateStrategy styleUpdateStrategy)
{
    ASSERT(element);
    if (styleUpdateStrategy == AllowStyleUpdate)
        element->document().updateLayoutIgnorePendingStylesheets();

    SVGElement* stopAtElement = mode == NearestViewportScope ? nearestViewportElement(element) : nullptr;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (element->document().settings().layerBasedSVGEngineEnabled()) {
        // Rudimentary support for operations on "detached" elements.
        auto* renderer = dynamicDowncast<RenderLayerModelObject>(element->renderer());
        if (!renderer)
            return element->localCoordinateSpaceTransform(mode);

        auto trackingMode { mode == SVGLocatable::ScreenScope ? TransformState::TrackSVGScreenCTMMatrix : TransformState::TrackSVGCTMMatrix };
        auto* stopAtRenderer = dynamicDowncast<RenderLayerModelObject>(stopAtElement ? stopAtElement->renderer() : nullptr);
        return SVGLayerTransformComputation(*renderer).computeAccumulatedTransform(stopAtRenderer, trackingMode);
    }
#endif

    AffineTransform ctm;

    for (Element* currentElement = element; currentElement; currentElement = currentElement->parentOrShadowHostElement()) {
        if (!currentElement->isSVGElement())
            break;

        ctm = downcast<SVGElement>(*currentElement).localCoordinateSpaceTransform(mode).multiply(ctm);

        // For getCTM() computation, stop at the nearest viewport element
        if (currentElement == stopAtElement)
            break;
    }

    return ctm;
}

ExceptionOr<Ref<SVGMatrix>> SVGLocatable::getTransformToElement(SVGElement* target, StyleUpdateStrategy styleUpdateStrategy)
{
    AffineTransform ctm = getCTM(styleUpdateStrategy);

    if (is<SVGGraphicsElement>(target)) {
        AffineTransform targetCTM = downcast<SVGGraphicsElement>(*target).getCTM(styleUpdateStrategy);
        if (auto inverse = targetCTM.inverse())
            ctm = inverse.value() * ctm;
        else
            return Exception { InvalidStateError, "Matrix is not invertible"_s };
    }

    return SVGMatrix::create(ctm);
}

}
