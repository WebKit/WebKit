/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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
#include "SVGPolyElement.h"

#include "DocumentInlines.h"
#include "LegacyRenderSVGPath.h"
#include "LegacyRenderSVGResource.h"
#include "RenderSVGPath.h"
#include "SVGDocumentExtensions.h"
#include "SVGParserUtilities.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGPolyElement);

SVGPolyElement::SVGPolyElement(const QualifiedName& tagName, Document& document)
    : SVGGeometryElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        PropertyRegistry::registerProperty<SVGNames::pointsAttr, &SVGPolyElement::m_points>();
    });
}

void SVGPolyElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::pointsAttr) {
        if (!m_points->baseVal()->parse(newValue))
            document().accessSVGExtensions().reportError("Problem parsing points=\"" + newValue + "\"");
    }

    SVGGeometryElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGPolyElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (PropertyRegistry::isKnownAttribute(attrName)) {
        ASSERT(attrName == SVGNames::pointsAttr);
        InstanceInvalidationGuard guard(*this);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (auto* path = dynamicDowncast<RenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();
#endif
        if (auto* path = dynamicDowncast<LegacyRenderSVGPath>(renderer()))
            path->setNeedsShapeUpdate();

        updateSVGRendererForElementChange();
        return;
    }

    SVGGeometryElement::svgAttributeChanged(attrName);
}

size_t SVGPolyElement::approximateMemoryCost() const
{
    size_t pointsCost = m_points->baseVal()->items().size() * sizeof(FloatPoint);

#if ENABLE(LAYER_BASED_SVG_ENGINE)
    if (document().settings().layerBasedSVGEngineEnabled()) {
        // We need to account for the memory which is allocated by the RenderSVGPath::m_path.
        return sizeof(*this) + (renderer() ? pointsCost * 2 + sizeof(RenderSVGPath) : pointsCost);
    }
#endif

    // We need to account for the memory which is allocated by the LegacyRenderSVGPath::m_path.
    return sizeof(*this) + (renderer() ? pointsCost * 2 + sizeof(LegacyRenderSVGPath) : pointsCost);
}

}
