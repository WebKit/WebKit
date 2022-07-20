/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.
 * Copyright (c) 2020, 2021, 2022 Igalia S.L.
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
#include "RenderSVGViewportContainer.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderLayer.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGRoot.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGViewportContainer);

RenderSVGViewportContainer::RenderSVGViewportContainer(Document& document, RenderStyle&& style)
    : RenderSVGContainer(document, WTFMove(style))
{
}

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGSVGElement& element, RenderStyle&& style)
    : RenderSVGContainer(element, WTFMove(style))
{
}

SVGSVGElement& RenderSVGViewportContainer::svgSVGElement() const
{
    if (isOutermostSVGViewportContainer()) {
        ASSERT(is<RenderSVGRoot>(parent()));
        return downcast<RenderSVGRoot>(*parent()).svgSVGElement();
    }

    return downcast<SVGSVGElement>(RenderSVGContainer::element());
}

FloatRect RenderSVGViewportContainer::computeViewport() const
{
    if (isOutermostSVGViewportContainer()) {
        ASSERT(is<RenderSVGRoot>(parent()));
        return { { }, downcast<RenderSVGRoot>(*parent()).computeViewportSize() };
    }

    auto& useSVGSVGElement = svgSVGElement();

    SVGLengthContext lengthContext(&useSVGSVGElement);
    return { useSVGSVGElement.x().value(lengthContext), useSVGSVGElement.y().value(lengthContext),
        useSVGSVGElement.width().value(lengthContext), useSVGSVGElement.height().value(lengthContext) };
}

bool RenderSVGViewportContainer::updateLayoutSizeIfNeeded()
{
    auto previousSize = m_viewport.size();
    m_viewport = computeViewport();
    return selfNeedsLayout() || (svgSVGElement().hasRelativeLengths() && previousSize != m_viewport.size());
}

void RenderSVGViewportContainer::updateFromElement()
{
    RenderSVGContainer::updateFromElement();
    updateLayerTransform();
}

void RenderSVGViewportContainer::updateFromStyle()
{
    RenderSVGContainer::updateFromStyle();

    if (SVGRenderSupport::isOverflowHidden(*this))
        setHasNonVisibleOverflow();

    if (hasSVGTransform() || !parent())
        return;

    auto& useSVGSVGElement = svgSVGElement();
    if (useSVGSVGElement.hasAttribute(SVGNames::viewBoxAttr) && !useSVGSVGElement.hasEmptyViewBox()) {
        setHasTransformRelatedProperty();
        setHasSVGTransform();
    }
}

void RenderSVGViewportContainer::updateLayerTransform()
{
    // First update the supplemental layer transform...
    auto& useSVGSVGElement = svgSVGElement();
    auto viewport = m_viewport;

    m_supplementalLayerTransform.makeIdentity();

    if (isOutermostSVGViewportContainer()) {
        // Handle pan - set on outermost <svg> element.
        if (auto translation = useSVGSVGElement.currentTranslateValue(); !translation.isZero())
            m_supplementalLayerTransform.translate(translation);

        // Handle zoom - take effective zoom from outermost <svg> element.
        if (auto scale = useSVGSVGElement.renderer()->style().effectiveZoom(); scale != 1) {
            m_supplementalLayerTransform.scale(scale);
            viewport.scale(1.0 / scale);
        }
    } else if (!viewport.location().isZero())
        m_supplementalLayerTransform.translate(viewport.location());

    auto viewBoxTransform = useSVGSVGElement.viewBoxToViewTransform(viewport.width(), viewport.height());
    if (!viewBoxTransform.isIdentity()) {
        if (m_supplementalLayerTransform.isIdentity())
            m_supplementalLayerTransform = viewBoxTransform;
        else
            m_supplementalLayerTransform.multiply(viewBoxTransform);
    }

    // ... before being able to use it in RenderLayerModelObjects::updateLayerTransform().
    RenderSVGContainer::updateLayerTransform();

    // An empty viewBox disables the rendering -- dirty the visible descendant status!
    if (hasLayer() && useSVGSVGElement.hasAttribute(SVGNames::viewBoxAttr) && useSVGSVGElement.hasEmptyViewBox())
        layer()->dirtyVisibleContentStatus();
}

void RenderSVGViewportContainer::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    applySVGTransform(transform, svgSVGElement(), style, boundingBox, std::make_optional(m_supplementalLayerTransform), options);
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
