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
        virtual void synchronizeProperty(const QualifiedName&);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        RadialGradientAttributes collectGradientProperties();
        void calculateFocalCenterPointsAndRadius(const RadialGradientAttributes&, FloatPoint& focalPoint, FloatPoint& centerPoint, float& radius);

    private:
        DECLARE_ANIMATED_PROPERTY(SVGRadialGradientElement, SVGNames::cxAttr, SVGLength, Cx, cx)
        DECLARE_ANIMATED_PROPERTY(SVGRadialGradientElement, SVGNames::cyAttr, SVGLength, Cy, cy)
        DECLARE_ANIMATED_PROPERTY(SVGRadialGradientElement, SVGNames::rAttr, SVGLength, R, r)
        DECLARE_ANIMATED_PROPERTY(SVGRadialGradientElement, SVGNames::fxAttr, SVGLength, Fx, fx)
        DECLARE_ANIMATED_PROPERTY(SVGRadialGradientElement, SVGNames::fyAttr, SVGLength, Fy, fy)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
