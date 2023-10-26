/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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
#include "SVGViewElement.h"

#include "LegacyRenderSVGResource.h"
#include "RenderElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "SVGStringList.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGViewElement);

inline SVGViewElement::SVGViewElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
    , SVGFitToViewBox(this)
{
    ASSERT(hasTagName(SVGNames::viewTag));
}

Ref<SVGViewElement> SVGViewElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGViewElement(tagName, document));
}

void SVGViewElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    SVGFitToViewBox::parseAttribute(name, newValue);
    SVGZoomAndPan::parseAttribute(name, newValue);
    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGViewElement::svgAttributeChanged(const QualifiedName& attrName)
{
    // We ignore changes to SVGNames::viewTargetAttr, which is deprecated and unused in WebCore.
    if (PropertyRegistry::isKnownAttribute(attrName))
        return;

    if (SVGFitToViewBox::isKnownAttribute(attrName)) {
        if (!m_targetElement)
            return;
        m_targetElement->inheritViewAttributes(*this);
        m_targetElement->updateSVGRendererForElementChange();
        return;
    }

    SVGElement::svgAttributeChanged(attrName);
}

}
