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

#pragma once

#include "SVGGeometryElement.h"
#include "SVGNames.h"

namespace WebCore {

class SVGPolyElement : public SVGGeometryElement {
    WTF_MAKE_ISO_ALLOCATED(SVGPolyElement);
public:
    const SVGPointList& points() const { return m_points->currentValue(); }

    SVGPointList& points() { return m_points->baseVal(); }
    SVGPointList& animatedPoints() { return *m_points->animVal(); }

    size_t approximateMemoryCost() const override;

protected:
    SVGPolyElement(const QualifiedName&, Document&);

private:
    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGPolyElement, SVGGeometryElement>;

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool isValid() const override { return SVGTests::isValid(); }
    bool supportsMarkers() const override { return true; }

    Ref<SVGAnimatedPointList> m_points { SVGAnimatedPointList::create(this) };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGPolyElement)
    static bool isType(const WebCore::SVGElement& element) { return element.hasTagName(WebCore::SVGNames::polygonTag) || element.hasTagName(WebCore::SVGNames::polylineTag); }
    static bool isType(const WebCore::Node& node)
    {
        auto* svgElement = dynamicDowncast<WebCore::SVGElement>(node);
        return svgElement && isType(*svgElement);
    }
SPECIALIZE_TYPE_TRAITS_END()
