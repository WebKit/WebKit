/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#pragma once

#include "Gradient.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedTransformList.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGNames.h"
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

enum SVGSpreadMethodType {
    SVGSpreadMethodUnknown = 0,
    SVGSpreadMethodPad,
    SVGSpreadMethodReflect,
    SVGSpreadMethodRepeat
};

template<>
struct SVGPropertyTraits<SVGSpreadMethodType> {
    static unsigned highestEnumValue() { return SVGSpreadMethodRepeat; }

    static String toString(SVGSpreadMethodType type)
    {
        switch (type) {
        case SVGSpreadMethodUnknown:
            return emptyString();
        case SVGSpreadMethodPad:
            return "pad"_s;
        case SVGSpreadMethodReflect:
            return "reflect"_s;
        case SVGSpreadMethodRepeat:
            return "repeat"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGSpreadMethodType fromString(const String& value)
    {
        if (value == "pad")
            return SVGSpreadMethodPad;
        if (value == "reflect")
            return SVGSpreadMethodReflect;
        if (value == "repeat")
            return SVGSpreadMethodRepeat;
        return SVGSpreadMethodUnknown;
    }
};

class SVGGradientElement : public SVGElement, public SVGExternalResourcesRequired, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGGradientElement);
public:
    enum {
        SVG_SPREADMETHOD_UNKNOWN = SVGSpreadMethodUnknown,
        SVG_SPREADMETHOD_PAD = SVGSpreadMethodReflect,
        SVG_SPREADMETHOD_REFLECT = SVGSpreadMethodRepeat,
        SVG_SPREADMETHOD_REPEAT = SVGSpreadMethodUnknown
    };

    Vector<Gradient::ColorStop> buildStops();

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGGradientElement, SVGElement, SVGExternalResourcesRequired, SVGURIReference>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    SVGSpreadMethodType spreadMethod() const { return m_spreadMethod.currentValue(attributeOwnerProxy()); }
    SVGUnitTypes::SVGUnitType gradientUnits() const { return m_gradientUnits.currentValue(attributeOwnerProxy()); }
    const SVGTransformListValues& gradientTransform() const { return m_gradientTransform.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedEnumeration> spreadMethodAnimated() { return m_spreadMethod.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> gradientUnitsAnimated() { return m_gradientUnits.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedTransformList> gradientTransformAnimated() { return m_gradientTransform.animatedProperty(attributeOwnerProxy()); }

protected:
    SVGGradientElement(const QualifiedName&, Document&);

    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

private:
    bool needsPendingResourceHandling() const override { return false; }
    void childrenChanged(const ChildChange&) override;

    const SVGAttributeOwnerProxy& attributeOwnerProxy() const override { return m_attributeOwnerProxy; }
    static void registerAttributes();

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedEnumerationAttribute<SVGSpreadMethodType> m_spreadMethod { SVGSpreadMethodPad };
    SVGAnimatedEnumerationAttribute<SVGUnitTypes::SVGUnitType> m_gradientUnits { SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX };
    SVGAnimatedTransformListAttribute m_gradientTransform;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGGradientElement)
static bool isType(const WebCore::SVGElement& element)
{
    return element.hasTagName(WebCore::SVGNames::radialGradientTag) || element.hasTagName(WebCore::SVGNames::linearGradientTag);
}
static bool isType(const WebCore::Node& node)
{
    return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node));
}
SPECIALIZE_TYPE_TRAITS_END()
