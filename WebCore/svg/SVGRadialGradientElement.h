/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGRadialGradientElement_h
#define SVGRadialGradientElement_h

#if ENABLE(SVG)
#include "SVGGradientElement.h"

namespace WebCore {

    struct RadialGradientAttributes;
    class SVGLength;

    class SVGRadialGradientElement : public SVGGradientElement {
    public:
        SVGRadialGradientElement(const QualifiedName&, Document*);
        virtual ~SVGRadialGradientElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

    protected:
        virtual void buildGradient() const;
        virtual SVGPaintServerType gradientType() const { return RadialGradientPaintServer; }

        RadialGradientAttributes collectGradientProperties() const;

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGNames::radialGradientTagString, SVGNames::cxAttrString, SVGLength, Cx, cx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGNames::radialGradientTagString, SVGNames::cyAttrString, SVGLength, Cy, cy)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGNames::radialGradientTagString, SVGNames::rAttrString, SVGLength, R, r)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGNames::radialGradientTagString, SVGNames::fxAttrString, SVGLength, Fx, fx)
        ANIMATED_PROPERTY_DECLARATIONS(SVGRadialGradientElement, SVGNames::radialGradientTagString, SVGNames::fyAttrString, SVGLength, Fy, fy)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
