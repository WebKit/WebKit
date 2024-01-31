/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Google Inc. All rights reserved.
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
#include "LegacyRenderSVGResourceMarker.h"

#include "GraphicsContext.h"
#include "LegacyRenderSVGResourceMarkerInlines.h"
#include "LegacyRenderSVGRoot.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StackStats.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(LegacyRenderSVGResourceMarker);

LegacyRenderSVGResourceMarker::LegacyRenderSVGResourceMarker(SVGMarkerElement& element, RenderStyle&& style)
    : LegacyRenderSVGResourceContainer(Type::LegacySVGResourceMarker, element, WTFMove(style))
{
}

LegacyRenderSVGResourceMarker::~LegacyRenderSVGResourceMarker() = default;

void LegacyRenderSVGResourceMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    // Invalidate all resources if our layout changed.
    if (everHadLayout() && selfNeedsLayout())
        LegacyRenderSVGRoot::addResourceForClientInvalidation(this);

    // RenderSVGHiddenContainer overwrites layout(). We need the
    // layouting of LegacyRenderSVGContainer for calculating  local
    // transformations and repaint.
    LegacyRenderSVGContainer::layout();
}

void LegacyRenderSVGResourceMarker::removeAllClientsFromCacheIfNeeded(bool markForInvalidation, SingleThreadWeakHashSet<RenderObject>* visitedRenderers)
{
    markAllClientsForInvalidationIfNeeded(markForInvalidation ? LayoutAndBoundariesInvalidation : ParentOnlyInvalidation, visitedRenderers);
}

void LegacyRenderSVGResourceMarker::removeClientFromCache(RenderElement& client, bool markForInvalidation)
{
    markClientForInvalidation(client, markForInvalidation ? BoundariesInvalidation : ParentOnlyInvalidation);
}

void LegacyRenderSVGResourceMarker::applyViewportClip(PaintInfo& paintInfo)
{
    if (SVGRenderSupport::isOverflowHidden(*this))
        paintInfo.context().clip(m_viewport);
}

FloatRect LegacyRenderSVGResourceMarker::markerBoundaries(RepaintRectCalculation repaintRectCalculation, const AffineTransform& markerTransformation) const
{
    FloatRect coordinates = LegacyRenderSVGContainer::repaintRectInLocalCoordinates(repaintRectCalculation);

    // Map repaint rect into parent coordinate space, in which the marker boundaries have to be evaluated
    coordinates = localToParentTransform().mapRect(coordinates);

    return markerTransformation.mapRect(coordinates);
}

const AffineTransform& LegacyRenderSVGResourceMarker::localToParentTransform() const
{
    m_localToParentTransform = AffineTransform::makeTranslation(toFloatSize(m_viewport.location())) * viewportTransform();
    return m_localToParentTransform;
    // If this class were ever given a localTransform(), then the above would read:
    // return viewportTranslation * localTransform() * viewportTransform();
}

FloatPoint LegacyRenderSVGResourceMarker::referencePoint() const
{
    SVGLengthContext lengthContext(&markerElement());
    return FloatPoint(markerElement().refX().value(lengthContext), markerElement().refY().value(lengthContext));
}

std::optional<float> LegacyRenderSVGResourceMarker::angle() const
{
    if (markerElement().orientType() == SVGMarkerOrientAngle)
        return markerElement().orientAngle().value();
    return std::nullopt;
}

AffineTransform LegacyRenderSVGResourceMarker::markerTransformation(const FloatPoint& origin, float autoAngle, float strokeWidth) const
{
    bool useStrokeWidth = markerElement().markerUnits() == SVGMarkerUnitsStrokeWidth;

    AffineTransform transform;
    transform.translate(origin);
    transform.rotate(angle().value_or(autoAngle));
    transform = markerContentTransformation(transform, referencePoint(), useStrokeWidth ? strokeWidth : -1);
    return transform;
}

void LegacyRenderSVGResourceMarker::draw(PaintInfo& paintInfo, const AffineTransform& transform)
{
    // An empty viewBox disables rendering.
    if (markerElement().hasAttribute(SVGNames::viewBoxAttr) && markerElement().hasEmptyViewBox())
        return;

    PaintInfo info(paintInfo);
    GraphicsContextStateSaver stateSaver(info.context());
    info.applyTransform(transform);
    LegacyRenderSVGContainer::paint(info, IntPoint());
}

AffineTransform LegacyRenderSVGResourceMarker::markerContentTransformation(const AffineTransform& contentTransformation, const FloatPoint& origin, float strokeWidth) const
{
    // The 'origin' coordinate maps to SVGs refX/refY, given in coordinates relative to the viewport established by the marker
    FloatPoint mappedOrigin = viewportTransform().mapPoint(origin);

    AffineTransform transformation = contentTransformation;
    if (strokeWidth != -1)
        transformation.scaleNonUniform(strokeWidth, strokeWidth);

    transformation.translate(-mappedOrigin);
    return transformation;
}

AffineTransform LegacyRenderSVGResourceMarker::viewportTransform() const
{
    return markerElement().viewBoxToViewTransform(m_viewport.width(), m_viewport.height());
}

void LegacyRenderSVGResourceMarker::calcViewport()
{
    if (!selfNeedsLayout())
        return;

    SVGLengthContext lengthContext(&markerElement());
    float w = markerElement().markerWidth().value(lengthContext);
    float h = markerElement().markerHeight().value(lengthContext);
    m_viewport = FloatRect(0, 0, w, h);
}

}
