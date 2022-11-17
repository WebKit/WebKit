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

RenderSVGViewportContainer::RenderSVGViewportContainer(RenderSVGRoot& parent, RenderStyle&& style)
    : RenderSVGContainer(parent.document(), WTFMove(style))
    , m_owningSVGRoot(parent)
{
}

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGSVGElement& element, RenderStyle&& style)
    : RenderSVGContainer(element, WTFMove(style))
{
}

SVGSVGElement& RenderSVGViewportContainer::svgSVGElement() const
{
    if (isOutermostSVGViewportContainer()) {
        ASSERT(m_owningSVGRoot);
        return m_owningSVGRoot->svgSVGElement();
    }
    return downcast<SVGSVGElement>(RenderSVGContainer::element());
}

FloatPoint RenderSVGViewportContainer::computeViewportLocation() const
{
    if (isOutermostSVGViewportContainer())
        return { };

    auto& useSVGSVGElement = svgSVGElement();
    SVGLengthContext lengthContext(&useSVGSVGElement);
    return { useSVGSVGElement.x().value(lengthContext), useSVGSVGElement.y().value(lengthContext) };
}

FloatSize RenderSVGViewportContainer::computeViewportSize() const
{
    if (isOutermostSVGViewportContainer()) {
        ASSERT(is<RenderSVGRoot>(parent()));
        return downcast<RenderSVGRoot>(*parent()).computeViewportSize();
    }

    auto& useSVGSVGElement = svgSVGElement();
    SVGLengthContext lengthContext(&useSVGSVGElement);
    return { useSVGSVGElement.width().value(lengthContext), useSVGSVGElement.height().value(lengthContext) };
}

bool RenderSVGViewportContainer::updateLayoutSizeIfNeeded()
{
    auto previousViewportSize = viewportSize();
    m_viewport = { computeViewportLocation(), computeViewportSize() };
    return selfNeedsLayout() || previousViewportSize != viewportSize();
}

bool RenderSVGViewportContainer::needsHasSVGTransformFlags() const
{
    auto& useSVGSVGElement = svgSVGElement();
    if (useSVGSVGElement.hasTransformRelatedAttributes())
        return true;

    if (isOutermostSVGViewportContainer())
        return !useSVGSVGElement.currentTranslateValue().isZero() || useSVGSVGElement.renderer()->style().effectiveZoom() != 1;

    return false;
}

void RenderSVGViewportContainer::updateFromStyle()
{
    RenderSVGContainer::updateFromStyle();

    if (SVGRenderSupport::isOverflowHidden(*this))
        setHasNonVisibleOverflow();
}

inline AffineTransform viewBoxToViewTransform(const SVGSVGElement& svgSVGElement, const FloatSize& viewportSize)
{
    return svgSVGElement.viewBoxToViewTransform(viewportSize.width(), viewportSize.height());
}

void RenderSVGViewportContainer::updateLayerTransform()
{
    ASSERT(hasLayer());

    // First update the supplemental layer transform.
    auto& useSVGSVGElement = svgSVGElement();
    auto viewportSize = this->viewportSize();

    m_supplementalLayerTransform.makeIdentity();

    if (isOutermostSVGViewportContainer()) {
        // Handle pan - set on outermost <svg> element.
        if (auto translation = useSVGSVGElement.currentTranslateValue(); !translation.isZero())
            m_supplementalLayerTransform.translate(translation);

        // Handle zoom - take effective zoom from outermost <svg> element.
        if (auto scale = useSVGSVGElement.renderer()->style().effectiveZoom(); scale != 1) {
            m_supplementalLayerTransform.scale(scale);
            viewportSize.scale(1.0 / scale);
        }
    } else if (!m_viewport.location().isZero())
        m_supplementalLayerTransform.translate(m_viewport.location());

    if (useSVGSVGElement.hasAttribute(SVGNames::viewBoxAttr)) {
        // An empty viewBox disables the rendering -- dirty the visible descendant status!
        if (useSVGSVGElement.hasEmptyViewBox())
            layer()->dirtyVisibleContentStatus();
        else if (auto viewBoxTransform = viewBoxToViewTransform(useSVGSVGElement, viewportSize); !viewBoxTransform.isIdentity()) {
            if (m_supplementalLayerTransform.isIdentity())
                m_supplementalLayerTransform = viewBoxTransform;
            else
                m_supplementalLayerTransform.multiply(viewBoxTransform);
        }
    }

    // After updating the supplemental layer transform we're able to use it in RenderLayerModelObjects::updateLayerTransform().
    RenderSVGContainer::updateLayerTransform();
}

void RenderSVGViewportContainer::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    applySVGTransform(transform, svgSVGElement(), style, boundingBox, m_supplementalLayerTransform.isIdentity() ? std::nullopt : std::make_optional(m_supplementalLayerTransform), std::nullopt, options);
}

LayoutRect RenderSVGViewportContainer::overflowClipRect(const LayoutPoint& location, RenderFragmentContainer*, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    // Overflow for the outermost <svg> element is handled in RenderSVGRoot, not here.
    ASSERT(!isOutermostSVGViewportContainer());
    auto& useSVGSVGElement = svgSVGElement();

    auto clipRect = enclosingLayoutRect(viewport());
    if (useSVGSVGElement.hasAttribute(SVGNames::viewBoxAttr)) {
        if (useSVGSVGElement.hasEmptyViewBox())
            return { };

        if (auto viewBoxTransform = viewBoxToViewTransform(useSVGSVGElement, viewportSize()); !viewBoxTransform.isIdentity())
            clipRect = enclosingLayoutRect(viewBoxTransform.inverse().value_or(AffineTransform { }).mapRect(viewport()));
    }

    clipRect.moveBy(location);
    return clipRect;
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
