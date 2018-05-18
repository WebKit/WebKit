/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "SVGAnimatedBoolean.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGGeometryElement.h"
#include "SVGNames.h"
#include "SVGPointListValues.h"

namespace WebCore {

class SVGPolyElement : public SVGGeometryElement, public SVGExternalResourcesRequired {
    WTF_MAKE_ISO_ALLOCATED(SVGPolyElement);
public:
    Ref<SVGPointList> points();
    Ref<SVGPointList> animatedPoints();

    SVGPointListValues& pointList() const { return m_points.value; }

    static const SVGPropertyInfo* pointsPropertyInfo();

    size_t approximateMemoryCost() const override;

protected:
    SVGPolyElement(const QualifiedName&, Document&);

private:
    bool isValid() const override { return SVGTests::isValid(); }

    void parseAttribute(const QualifiedName&, const AtomicString&) override; 
    void svgAttributeChanged(const QualifiedName&) override;

    bool supportsMarkers() const override { return true; }

    // Custom 'points' property
    static void synchronizePoints(SVGElement* contextElement);
    static Ref<SVGAnimatedProperty> lookupOrCreatePointsWrapper(SVGElement* contextElement);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGPolyElement)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

protected:
    mutable SVGSynchronizableAnimatedProperty<SVGPointListValues> m_points;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGPolyElement)
    static bool isType(const WebCore::SVGElement& element) { return element.hasTagName(WebCore::SVGNames::polygonTag) || element.hasTagName(WebCore::SVGNames::polylineTag); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
