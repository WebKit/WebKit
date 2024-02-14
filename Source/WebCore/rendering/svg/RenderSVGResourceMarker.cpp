/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2022, 2023, 2024 Igalia S.L.
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
#include "RenderSVGResourceMarker.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "Element.h"
#include "ElementIterator.h"
#include "FloatPoint.h"
#include "Image.h"
#include "ImageBuffer.h"
#include "IntRect.h"
#include "RenderLayer.h"
#include "RenderLayerInlines.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGResourceMarkerInlines.h"
#include "SVGGraphicsElement.h"
#include "SVGLengthContext.h"
#include "SVGRenderStyle.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGResourceMarker);

RenderSVGResourceMarker::RenderSVGResourceMarker(SVGMarkerElement& element, RenderStyle&& style)
    : RenderSVGResourceContainer(Type::SVGResourceMarker, element, WTFMove(style))
{
}

RenderSVGResourceMarker::~RenderSVGResourceMarker() = default;

void RenderSVGResourceMarker::invalidateMarker()
{
    // Repainting all clients is sufficient to handle invalidation.
    repaintAllClients();
}

FloatRect RenderSVGResourceMarker::computeViewport() const
{
    auto& useMarkerElement = markerElement();
    SVGLengthContext lengthContext(&useMarkerElement);
    return { 0, 0, useMarkerElement.markerWidth().value(lengthContext), useMarkerElement.markerHeight().value(lengthContext) };
}

bool RenderSVGResourceMarker::updateLayoutSizeIfNeeded()
{
    auto previousViewportSize = viewportSize();
    m_viewport = computeViewport();
    return selfNeedsLayout() || previousViewportSize != viewportSize();
}

void RenderSVGResourceMarker::layout()
{
    // RenderSVGResourceContainer inherits from RenderSVGHiddenContainer which has a no-op layout() implementation.
    // Markers are always painted indirectly, but their children need to be laid out such as they would be regular
    // children of a SVG container element, thus skip RenderSVGHiddenContainer::layout() and use RenderSVGContainer::layout().
    RenderSVGContainer::layout();
}

void RenderSVGResourceMarker::updateFromStyle()
{
    RenderSVGContainer::updateFromStyle();

    if (SVGRenderSupport::isOverflowHidden(*this))
        setHasNonVisibleOverflow();
}

void RenderSVGResourceMarker::updateLayerTransform()
{
    ASSERT(hasLayer());

    // First update the supplemental layer transform.
    auto& useMarkerElement = markerElement();
    auto viewportSize = this->viewportSize();

    m_supplementalLayerTransform.makeIdentity();

    if (useMarkerElement.hasAttribute(SVGNames::viewBoxAttr)) {
        // An empty viewBox disables the rendering -- dirty the visible descendant status!
        if (useMarkerElement.hasEmptyViewBox())
            layer()->dirtyVisibleContentStatus();
        else if (auto viewBoxTransform = useMarkerElement.viewBoxToViewTransform(viewportSize.width(), viewportSize.height()); !viewBoxTransform.isIdentity())
            m_supplementalLayerTransform = viewBoxTransform;
    }

    // After updating the supplemental layer transform we're able to use it in RenderLayerModelObjects::updateLayerTransform().
    RenderSVGContainer::updateLayerTransform();
}

void RenderSVGResourceMarker::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    // This code resembles RenderLayerModelObject::applySVGTransform(), but supporting non-SVGGraphicsElement derived elements,
    // such as SVGMarkerElement, that do not allow user-specified transformations (no SMIL, no SVG/CSS transformations) - only
    // the marker-induced transformations when applying markers to path elements.
    FloatPoint3D originTranslate;
    if (options.contains(RenderStyle::TransformOperationOption::TransformOrigin) && !m_supplementalLayerTransform.isIdentityOrTranslation())
        originTranslate = style.computeTransformOrigin(boundingBox);

    style.applyTransformOrigin(transform, originTranslate);
    transform.multiplyAffineTransform(m_supplementalLayerTransform);
    style.unapplyTransformOrigin(transform, originTranslate);
}

LayoutRect RenderSVGResourceMarker::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer*, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    auto& useMarkerElement = markerElement();

    auto clipRect = enclosingLayoutRect(viewport());
    if (useMarkerElement.hasAttribute(SVGNames::viewBoxAttr)) {
        if (useMarkerElement.hasEmptyViewBox())
            return { };

        if (!m_supplementalLayerTransform.isIdentity())
            clipRect = enclosingLayoutRect(m_supplementalLayerTransform.inverse().value_or(AffineTransform { }).mapRect(viewport()));
    }

    clipRect.moveBy(location);
    return clipRect;
}

AffineTransform RenderSVGResourceMarker::markerTransformation(const FloatPoint& origin, float autoAngle, float strokeWidth) const
{
    AffineTransform transform;
    transform.translate(origin);
    transform.rotate(angle().value_or(autoAngle));

    // The 'referencePoint()' coordinate maps to SVGs refX/refY, given in coordinates relative to the viewport established by the marker
    auto mappedOrigin = m_supplementalLayerTransform.mapPoint(referencePoint());

    if (markerUnits() == SVGMarkerUnitsStrokeWidth)
        transform.scaleNonUniform(strokeWidth, strokeWidth);

    transform.translate(-mappedOrigin);
    return transform;
}

FloatRect RenderSVGResourceMarker::computeMarkerBoundingBox(const SVGBoundingBoxComputation::DecorationOptions& options, const AffineTransform& markerTransformation) const
{
    SVGBoundingBoxComputation boundingBoxComputation(*this);
    auto boundingBox = boundingBoxComputation.computeDecoratedBoundingBox(options);

    // Map repaint rect into parent coordinate space, in which the marker boundaries have to be evaluated
    return markerTransformation.mapRect(m_supplementalLayerTransform.mapRect(boundingBox));
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
