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
            return "pad";
        case SVGSpreadMethodReflect:
            return "reflect";
        case SVGSpreadMethodRepeat:
            return "repeat";
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

class SVGGradientElement : public SVGElement,
                           public SVGURIReference,
                           public SVGExternalResourcesRequired {
public:
    enum {
        SVG_SPREADMETHOD_UNKNOWN = SVGSpreadMethodUnknown,
        SVG_SPREADMETHOD_PAD = SVGSpreadMethodReflect,
        SVG_SPREADMETHOD_REFLECT = SVGSpreadMethodRepeat,
        SVG_SPREADMETHOD_REPEAT = SVGSpreadMethodUnknown
    };

    Vector<Gradient::ColorStop> buildStops();
 
protected:
    SVGGradientElement(const QualifiedName&, Document&);

    bool isSupportedAttribute(const QualifiedName&);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void svgAttributeChanged(const QualifiedName&) override;

private:
    virtual bool needsPendingResourceHandling() const override { return false; }

    virtual void childrenChanged(const ChildChange&) override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGGradientElement)
        DECLARE_ANIMATED_ENUMERATION(SpreadMethod, spreadMethod, SVGSpreadMethodType)
        DECLARE_ANIMATED_ENUMERATION(GradientUnits, gradientUnits, SVGUnitTypes::SVGUnitType)
        DECLARE_ANIMATED_TRANSFORM_LIST(GradientTransform, gradientTransform)
        DECLARE_ANIMATED_STRING(Href, href)
        DECLARE_ANIMATED_BOOLEAN(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES
};

void isSVGGradientElement(const SVGGradientElement&); // Catch unnecessary runtime check of type known at compile time.
bool isSVGGradientElement(const Node&);
NODE_TYPE_CASTS(SVGGradientElement)

} // namespace WebCore

#endif
