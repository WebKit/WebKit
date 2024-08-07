/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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
#include "SVGMaskElement.h"

#include "LegacyRenderSVGResourceMaskerInlines.h"
#include "NodeName.h"
#include "RenderElementInlines.h"
#include "RenderSVGResourceMasker.h"
#include "SVGElementInlines.h"
#include "SVGLayerTransformComputation.h"
#include "SVGNames.h"
#include "SVGRenderSupport.h"
#include "SVGStringList.h"
#include "SVGUnitTypes.h"
#include "StyleResolver.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGMaskElement);

inline SVGMaskElement::SVGMaskElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGTests(this)
{
    // Spec: If the x/y attribute is not specified, the effect is as if a value of "-10%" were specified.
    // Spec: If the width/height attribute is not specified, the effect is as if a value of "120%" were specified.
    ASSERT(hasTagName(SVGNames::maskTag));

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::xAttr, &SVGMaskElement::m_x>();
        PropertyRegistry::registerProperty<SVGNames::yAttr, &SVGMaskElement::m_y>();
        PropertyRegistry::registerProperty<SVGNames::widthAttr, &SVGMaskElement::m_width>();
        PropertyRegistry::registerProperty<SVGNames::heightAttr, &SVGMaskElement::m_height>();
        PropertyRegistry::registerProperty<SVGNames::maskUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGMaskElement::m_maskUnits>();
        PropertyRegistry::registerProperty<SVGNames::maskContentUnitsAttr, SVGUnitTypes::SVGUnitType, &SVGMaskElement::m_maskContentUnits>();
    });
}

Ref<SVGMaskElement> SVGMaskElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGMaskElement(tagName, document));
}

void SVGMaskElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGParsingError parseError = NoError;
    switch (name.nodeName()) {
    case AttributeNames::maskUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_maskUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::maskContentUnitsAttr: {
        auto propertyValue = SVGPropertyTraits<SVGUnitTypes::SVGUnitType>::fromString(newValue);
        if (propertyValue > 0)
            Ref { m_maskContentUnits }->setBaseValInternal<SVGUnitTypes::SVGUnitType>(propertyValue);
        break;
    }
    case AttributeNames::xAttr:
        Ref { m_x }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::yAttr:
        Ref { m_y }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    case AttributeNames::widthAttr:
        Ref { m_width }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Width, newValue, parseError));
        break;
    case AttributeNames::heightAttr:
        Ref { m_height }->setBaseValInternal(SVGLengthValue::construct(SVGLengthMode::Height, newValue, parseError));
        break;
    default:
        break;
    }
    reportAttributeParsingError(parseError, name, newValue);

    SVGTests::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGMaskElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isAnimatedLengthAttribute(attrName)) {
        InstanceInvalidationGuard guard(*this);
        setPresentationalHintStyleIsDirty();
        return;
    }

    if (PropertyRegistry::isKnownAttribute(attrName)) {
        if (document().settings().layerBasedSVGEngineEnabled()) {
            if (CheckedPtr maskRenderer = dynamicDowncast<RenderSVGResourceMasker>(renderer())) {
                maskRenderer->invalidateMask();
                maskRenderer->repaintClientsOfReferencedSVGResources();
                return;
            }
        }

        updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

void SVGMaskElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (change.source == ChildChange::Source::Parser)
        return;

    if (document().settings().layerBasedSVGEngineEnabled()) {
        if (CheckedPtr maskRenderer = dynamicDowncast<RenderSVGResourceMasker>(renderer())) {
            maskRenderer->invalidateMask();
            maskRenderer->repaintClientsOfReferencedSVGResources();
        }
    }

    updateSVGRendererForElementChange();
}

RenderPtr<RenderElement> SVGMaskElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (document().settings().layerBasedSVGEngineEnabled())
        return createRenderer<RenderSVGResourceMasker>(*this, WTFMove(style));
    return createRenderer<LegacyRenderSVGResourceMasker>(*this, WTFMove(style));
}

FloatRect SVGMaskElement::calculateMaskContentRepaintRect(RepaintRectCalculation repaintRectCalculation)
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
    FloatRect maskRepaintRect;
    for (auto* childNode = firstChild(); childNode; childNode = childNode->nextSibling()) {
        CheckedPtr renderer = childNode->renderer();
        if (!childNode->isSVGElement() || !renderer)
            continue;
        const auto& style = renderer->style();
        if (style.display() == DisplayType::None || style.usedVisibility() != Visibility::Visible)
            continue;
        auto r = renderer->repaintRectInLocalCoordinates(repaintRectCalculation);
        if (auto transform = transformationMatrixFromChild(downcast<RenderLayerModelObject>(*renderer)))
            r = transform->mapRect(r);
        maskRepaintRect.unite(r);
    }
    return maskRepaintRect;
}

}
