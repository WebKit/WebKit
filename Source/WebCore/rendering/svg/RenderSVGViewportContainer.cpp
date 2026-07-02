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

#include "ContainerNodeInlines.h"
#include "RenderLayer.h"
#include "RenderObjectDocument.h"
#include "RenderSVGModelObjectInlines.h"
#include "RenderSVGRoot.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "SVGViewSpec.h"
#include "StyleComputedStyle+GettersInlines.h"
#include <wtf/TZoneMallocInlines.h>
#include "RenderObjectNode.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderSVGViewportContainer);

RenderSVGViewportContainer::RenderSVGViewportContainer(RenderSVGRoot& parent, Style::ComputedStyle&& style)
    : RenderSVGContainer(Type::SVGViewportContainer, parent.document(), WTF::move(style))
    , m_owningSVGRoot(parent)
{
    ASSERT(isRenderSVGViewportContainer());
}

RenderSVGViewportContainer::RenderSVGViewportContainer(SVGSVGElement& element, Style::ComputedStyle&& style)
    : RenderSVGContainer(Type::SVGViewportContainer, element, WTF::move(style))
{
    ASSERT(isRenderSVGViewportContainer());
}

RenderSVGViewportContainer::~RenderSVGViewportContainer() = default;

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

    Ref useSVGSVGElement = svgSVGElement();
    SVGLengthContext lengthContext(useSVGSVGElement.ptr());
    return { useSVGSVGElement->x().value(lengthContext), useSVGSVGElement->y().value(lengthContext) };
}

FloatSize RenderSVGViewportContainer::computeViewportSize() const
{
    if (isOutermostSVGViewportContainer())
        return downcast<RenderSVGRoot>(*parent()).computeViewportSize();

    Ref useSVGSVGElement = svgSVGElement();
    SVGLengthContext lengthContext(useSVGSVGElement.ptr());
    return { useSVGSVGElement->width().value(lengthContext), useSVGSVGElement->height().value(lengthContext) };
}

bool RenderSVGViewportContainer::updateLayoutSizeIfNeeded()
{
    auto previousViewportSize = viewportSize();
    m_viewport = { computeViewportLocation(), computeViewportSize() };
    return selfNeedsLayout() || previousViewportSize != viewportSize();
}

bool RenderSVGViewportContainer::needsHasSVGTransformFlags() const
{
    Ref useSVGSVGElement = svgSVGElement();
    if (useSVGSVGElement->hasTransformRelatedAttributes())
        return true;

    if (isOutermostSVGViewportContainer()) {
        if (useSVGSVGElement->useCurrentView())
            return true;
        if (!useSVGSVGElement->currentTranslateValue().isZero() || useSVGSVGElement->renderer()->style().usedZoom() != 1)
            return true;
        // Need transform flags when the SVG root has a non-zero content box location
        // (e.g., due to border or padding), so that the content box offset is propagated
        // through the transform painting chain to transformed descendants.
        // Use m_owningSVGRoot instead of parent() since this may be called during
        // initializeStyle() before the renderer is attached to the tree.
        if (m_owningSVGRoot)
            return !m_owningSVGRoot->contentBoxLocation().isZero();
    }

    return false;
}

void RenderSVGViewportContainer::updateFromStyle()
{
    RenderSVGContainer::updateFromStyle();

    // Overflow for the outermost <svg> element is handled by RenderSVGRoot, not here. Even though the
    // outermost (anonymous) viewport container always takes a layer, it must not install its own overflow
    // clip, which would clip (and corrupt the clip rects of, e.g. filter backgrounds for) descendants
    // through a coordinate space RenderSVGRoot already accounts for.
    if (!isOutermostSVGViewportContainer() && SVGRenderSupport::isOverflowHidden(*this))
        setHasNonVisibleOverflow();
}

inline AffineTransform viewBoxToViewTransform(const SVGSVGElement& svgSVGElement, const FloatSize& viewportSize)
{
    return svgSVGElement.viewBoxToViewTransform(viewportSize.width(), viewportSize.height());
}

void RenderSVGViewportContainer::updateLayerTransform()
{
    // First update the supplemental layer transform.
    Ref useSVGSVGElement = svgSVGElement();
    auto viewportSize = this->viewportSize();

    m_supplementalLayerTransform.makeIdentity();

    if (isOutermostSVGViewportContainer()) {
        // Handle pan - set on outermost <svg> element.
        if (auto translation = useSVGSVGElement->currentTranslateValue(); !translation.isZero())
            m_supplementalLayerTransform.translate(translation);

        // Handle zoom - take effective zoom from outermost <svg> element.
        CheckedRef svgRoot = downcast<RenderSVGRoot>(*useSVGSVGElement->renderer());
        if (auto scale = svgRoot->style().usedZoom(); scale != 1) {
            // The outermost viewport container is positioned at the content box origin
            // (border + padding offset from the SVG root's border box). In paintLayerByApplyingTransform(),
            // the layer's positional offset is applied via translateRight AFTER the supplemental transform.
            // Without compensation, the zoom scale would be applied to this positional offset, causing
            // the SVG content to be shifted by (zoom - 1) * contentBoxOffset. Wrap the scale with
            // compensating translations to prevent the positional offset from being scaled.
            auto contentBoxOffset = svgRoot->contentBoxLocation();
            if (!contentBoxOffset.isZero()) {
                auto cbx = contentBoxOffset.x().toFloat();
                auto cby = contentBoxOffset.y().toFloat();
                m_supplementalLayerTransform.translate(cbx, cby);
                m_supplementalLayerTransform.scale(scale);
                m_supplementalLayerTransform.translate(-cbx, -cby);
            } else
                m_supplementalLayerTransform.scale(scale);
            viewportSize.scale(1.0 / scale);
        }
    } else if (!m_viewport.location().isZero())
        m_supplementalLayerTransform.translate(m_viewport.location());

    bool hasCurrentViewEmptyViewBox = true;
    if (useSVGSVGElement->useCurrentView())
        hasCurrentViewEmptyViewBox = useSVGSVGElement->currentView().hasEmptyViewBox();
    if (useSVGSVGElement->hasAttribute(SVGNames::viewBoxAttr) || !hasCurrentViewEmptyViewBox) {
        // An empty viewBox disables the rendering -- dirty the visible descendant status!
        if (useSVGSVGElement->hasEmptyViewBox() && hasCurrentViewEmptyViewBox) {
            if (hasLayer())
                layer()->dirtyVisibleContentStatus();
        } else if (!useSVGSVGElement->viewBox().isEmpty() || !hasCurrentViewEmptyViewBox) {
            if (auto viewBoxTransform = viewBoxToViewTransform(useSVGSVGElement, viewportSize); !viewBoxTransform.isIdentity()) {
                if (m_supplementalLayerTransform.isIdentity())
                    m_supplementalLayerTransform = viewBoxTransform;
                else
                    m_supplementalLayerTransform.multiply(viewBoxTransform);
            }
        }
    }

    // After updating the supplemental layer transform we're able to use it in RenderLayerModelObjects::updateLayerTransform().
    RenderSVGContainer::updateLayerTransform();
}

void RenderSVGViewportContainer::applyTransform(TransformationMatrix& transform, const Style::ComputedStyle& style, const FloatRect& boundingBox, OptionSet<Style::TransformResolverOption> options) const
{
    applySVGTransform(transform, protect(svgSVGElement()), style, boundingBox, m_supplementalLayerTransform.isIdentity() ? std::nullopt : std::make_optional(m_supplementalLayerTransform), std::nullopt, options);
}

LayoutRect RenderSVGViewportContainer::overflowClipRect(const LayoutPoint& location, OverlayScrollbarSizeRelevancy, PaintPhase) const
{
    // The outermost <svg> element's overflow is clipped by RenderSVGRoot, not here. That viewport container
    // never sets up an overflow clip (see updateFromStyle), so this code normally is not reached for it.
    // Return an infinite rect, which clips nothing, in case it is.
    if (isOutermostSVGViewportContainer())
        return LayoutRect::infiniteRect();
    Ref useSVGSVGElement = svgSVGElement();

    auto clipRect = enclosingLayoutRect(viewport());
    if (useSVGSVGElement->hasAttribute(SVGNames::viewBoxAttr)) {
        if (useSVGSVGElement->hasEmptyViewBox())
            return { };

        if (!useSVGSVGElement->viewBox().isEmpty()) {
            if (auto viewBoxTransform = viewBoxToViewTransform(useSVGSVGElement, viewportSize()); !viewBoxTransform.isIdentity())
                clipRect = enclosingLayoutRect(viewBoxTransform.inverse().value_or(AffineTransform { }).mapRect(viewport()));
        }
    }

    clipRect.moveBy(location);
    return clipRect;
}

}

