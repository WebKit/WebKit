/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGGradientElement_h
#define SVGGradientElement_h

#if ENABLE(SVG)
#include "Gradient.h"
#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedTransformList.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"
#include "SVGUnitTypes.h"

namespace WebCore {

class SVGGradientElement : public SVGStyledElement,
                           public SVGURIReference,
                           public SVGExternalResourcesRequired {
public:
    enum SVGSpreadMethodType {
        SVG_SPREADMETHOD_UNKNOWN = 0,
        SVG_SPREADMETHOD_PAD     = 1,
        SVG_SPREADMETHOD_REFLECT = 2,
        SVG_SPREADMETHOD_REPEAT  = 3
    };

    Vector<Gradient::ColorStop> buildStops();
 
protected:
    SVGGradientElement(const QualifiedName&, Document*);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    void fillPassedAttributeToPropertyTypeMap(AttributeToPropertyTypeMap&);

private:
    virtual bool needsPendingResourceHandling() const { return false; }

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    // Animated property declarations
    DECLARE_ANIMATED_ENUMERATION(SpreadMethod, spreadMethod, SVGSpreadMethodType)
    DECLARE_ANIMATED_ENUMERATION(GradientUnits, gradientUnits, SVGUnitTypes::SVGUnitType)
    DECLARE_ANIMATED_TRANSFORM_LIST(GradientTransform, gradientTransform)

    // SVGURIReference
    DECLARE_ANIMATED_STRING(Href, href)

    // SVGExternalResourcesRequired
    DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
};

template<>
struct SVGPropertyTraits<SVGGradientElement::SVGSpreadMethodType> {
    static SVGGradientElement::SVGSpreadMethodType highestEnumValue() { return SVGGradientElement::SVG_SPREADMETHOD_REPEAT; }

    static String toString(SVGGradientElement::SVGSpreadMethodType type)
    {
        switch (type) {
        case SVGGradientElement::SVG_SPREADMETHOD_UNKNOWN:
            return emptyString();
        case SVGGradientElement::SVG_SPREADMETHOD_PAD:
            return "pad";
        case SVGGradientElement::SVG_SPREADMETHOD_REFLECT:
            return "reflect";
        case SVGGradientElement::SVG_SPREADMETHOD_REPEAT:
            return "repeat";
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGGradientElement::SVGSpreadMethodType fromString(const String& value)
    {
        if (value == "pad")
            return SVGGradientElement::SVG_SPREADMETHOD_PAD;
        if (value == "reflect")
            return SVGGradientElement::SVG_SPREADMETHOD_REFLECT;
        if (value == "repeat")
            return SVGGradientElement::SVG_SPREADMETHOD_REPEAT;
        return SVGGradientElement::SVG_SPREADMETHOD_UNKNOWN;
    }
};

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
