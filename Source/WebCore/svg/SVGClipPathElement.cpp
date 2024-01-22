/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "SVGClipPathElement.h"

#include "Document.h"
#include "ImageBuffer.h"
#include "LegacyRenderSVGResourceClipper.h"
#include "RenderElementInlines.h"
#include "RenderSVGResourceClipper.h"
#include "RenderSVGText.h"
#include "RenderStyleInlines.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGLayerTransformComputation.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#include "StyleResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGClipPathElement);

inline SVGClipPathElement::SVGClipPathElement(const QualifiedName& tagName, Document& document)
    : SVGGraphicsElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(SVGNames::clipPathTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::clipPathUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGClipPathElement::m_clipPathUnits>();
    });}

Ref<SVGClipPathElement> SVGClipPathElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGClipPathElement(tagName, document));
}

void SVGClipPathElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::clipPathUnitsAttr) {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            m_clipPathUnits->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
    }

    SVGGraphicsElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGClipPathElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (document().settings().layerBasedSVGEngineEnabled()) {
            if (CheckedPtr renderer = this->renderer())
                renderer->repaintClientsOfReferencedSVGResources();
            return;
        }
#endif

        updateSVGRendererForElementChange();
        return;
    }

    SVGGraphicsElement::svgAttributeChanged(attrName);
}

void SVGClipPathElement::childrenChanged(const ChildChange& change)
{
    SVGGraphicsElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr renderer = this->renderer())
            renderer->repaintClientsOfReferencedSVGResources();
        return;
    }
#endif

    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGClipPathElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourceClipper>(*this, WTFMove(style));
#endif
    return createRenderer<LegacyRenderSVGResourceClipper>(*this, WTFMove(style));
}

SVGGraphicsElement* SVGClipPathElement::shouldApplyPathClipping() const
{
    // If the current clip-path gets clipped itself, we have to fall back to masking.
    if (renderer() && renderer()->style().clipPath())
        return nullptr;

    auto rendererRequiresMaskClipping = [](const RenderObject& renderer) -> bool {
        // Only shapes or paths are supported for direct clipping. We need to fall back to masking for texts.
        if (is<RenderSVGText>(renderer))
            return true;
        auto& style = renderer.style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            return false;
        // Current shape in clip-path gets clipped too. Fall back to masking.
        return style.clipPath();
    };

    SVGGraphicsElement* useGraphicsElement = nullptr;

    // If clip-path only contains one visible shape or path, we can use path-based clipping. Invisible
    // shapes don't affect the clipping and can be ignored. If clip-path contains more than one
    // visible shape, the additive clipping may not work, caused by the clipRule. EvenOdd
    // as well as NonZero can cause self-clipping of the elements.
    // See also http://www.w3.org/TR/SVG/painting.html#FillRuleProperty
    for (auto* childNode = firstChild(); childNode; childNode = childNode->nextSibling()) {
        auto* graphicsElement = dynamicDowncast<SVGGraphicsElement>(*childNode);
        if (!graphicsElement)
            continue;
        auto* renderer = graphicsElement->renderer();
        if (!renderer)
            continue;
        if (rendererRequiresMaskClipping(*renderer))
            return nullptr;
        // Fallback to masking, if there is more than one clipping path.
        if (useGraphicsElement)
            return nullptr;

        // For <use> elements, delegate the decision whether to use mask clipping or not to the referenced element.
        if (auto* useElement = dynamicDowncast<SVGUseElement>(graphicsElement)) {
            auto* clipChildRenderer = useElement->rendererClipChild();
            if (clipChildRenderer && rendererRequiresMaskClipping(*clipChildRenderer))
                return nullptr;
        }

        useGraphicsElement = graphicsElement;
    }

    return useGraphicsElement;
}

FloatRect SVGClipPathElement::calculateClipContentRepaintRect(RepaintRectCalculation repaintRectCalculation)
{
    ASSERT(renderer());
    auto transformationMatrixFromChild = [&](const RenderLayerModelObject& child) -> std::optional<AffineTransform> {
        if (!document().settings().layerBasedSVGEngineEnabled())
            return std::nullopt;

        if (!(renderer()->isTransformed() || child.isTransformed()) || !child.hasLayer())
            return std::nullopt;

        ASSERT(child.isSVGLayerAwareRenderer());
        ASSERT(!child.isRenderSVGRoot());

        auto transform = SVGLayerTransformComputation(child).computeAccumulatedTransform(downcast<RenderLayerModelObject>(renderer()), TransformState::TrackSVGCTMMatrix);
        return transform.isIdentity() ? std::nullopt : std::make_optional(WTFMove(transform));
    };

    FloatRect clipContentRepaintRect;
    // This is a rough heuristic to appraise the clip size and doesn't consider clip on clip.
    for (auto* childNode = firstChild(); childNode; childNode = childNode->nextSibling()) {
        auto* renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        if (!renderer->isRenderSVGShape() && !renderer->isRenderSVGText() && !childNode->hasTagName(SVGNames::useTag))
            continue;
        auto& style = renderer->style();
        if (style.display() == DisplayType::None || style.visibility() != Visibility::Visible)
            continue;
        auto r = renderer->repaintRectInLocalCoordinates(repaintRectCalculation);
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto transform = transformationMatrixFromChild(downcast<RenderLayerModelObject>(*renderer)))
            r = transform->mapRect(r);
#endif
        clipContentRepaintRect.unite(r);
    }
    return clipContentRepaintRect;
}

}
