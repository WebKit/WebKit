/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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
#include "RenderSVGTransformableContainer.h"

#if ENABLE(LAYER_BASED_SVG_ENGINE)
#include "RenderSVGModelObjectInlines.h"
#include "SVGContainerLayout.h"
#include "SVGElementTypeHelpers.h"
#include "SVGGElement.h"
#include "SVGGraphicsElement.h"
#include "SVGUseElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderSVGTransformableContainer);

RenderSVGTransformableContainer::RenderSVGTransformableContainer(SVGGraphicsElement& element, RenderStyle&& style)
    : RenderSVGContainer(Type::SVGTransformableContainer, element, WTFMove(style))
{
    ASSERT(isRenderSVGTransformableContainer());
}

SVGGraphicsElement& RenderSVGTransformableContainer::graphicsElement() const
{
    return downcast<SVGGraphicsElement>(RenderSVGContainer::element());
}

inline SVGUseElement* associatedUseElement(SVGGraphicsElement& element)
{
    // If we're either the renderer for a <use> element, or for any <g> element inside the shadow
    // tree, that was created during the use/symbol/svg expansion in SVGUseElement. These containers
    // need to respect the translations induced by their corresponding use elements x/y attributes.
    if (auto* useElement = dynamicDowncast<SVGUseElement>(element))
        return useElement;

    if (element.isInShadowTree() && is<SVGGElement>(element)) {
        SVGElement* correspondingElement = element.correspondingElement();
        if (auto* useElement = dynamicDowncast<SVGUseElement>(correspondingElement))
            return useElement;
    }

    return nullptr;
}

FloatSize RenderSVGTransformableContainer::additionalContainerTranslation() const
{
    if (auto* useElement = associatedUseElement(graphicsElement())) {
        SVGLengthContext lengthContext(&graphicsElement());
        return { useElement->x().value(lengthContext), useElement->y().value(lengthContext) };
    }

    return { };
}

bool RenderSVGTransformableContainer::needsHasSVGTransformFlags() const
{
    return graphicsElement().hasTransformRelatedAttributes() || associatedUseElement(graphicsElement());
}

void RenderSVGTransformableContainer::updateLayerTransform()
{
    // First update the supplemental layer transform...
    m_supplementalLayerTransform = AffineTransform::makeTranslation(additionalContainerTranslation());

    // ... before being able to use it in RenderLayerModelObject::updateLayerTransform().
    RenderSVGContainer::updateLayerTransform();
}

void RenderSVGTransformableContainer::applyTransform(TransformationMatrix& transform, const RenderStyle& style, const FloatRect& boundingBox, OptionSet<RenderStyle::TransformOperationOption> options) const
{
    auto postTransform = m_supplementalLayerTransform.isIdentity() ? std::nullopt : std::make_optional(m_supplementalLayerTransform);
    applySVGTransform(transform, graphicsElement(), style, boundingBox, std::nullopt, postTransform, options);
}

}

#endif // ENABLE(LAYER_BASED_SVG_ENGINE)
