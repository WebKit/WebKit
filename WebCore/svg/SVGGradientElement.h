/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

    This file is part of the KDE project

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

#ifndef SVGGradientElement_h
#define SVGGradientElement_h

#if ENABLE(SVG)
#include "SVGPaintServerGradient.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

    class SVGGradientElement;
    class SVGTransformList;

    class SVGGradientElement : public SVGStyledElement,
                               public SVGURIReference,
                               public SVGExternalResourcesRequired {
    public:
        enum SVGGradientType {
            SVG_SPREADMETHOD_UNKNOWN = 0,
            SVG_SPREADMETHOD_PAD     = 1,
            SVG_SPREADMETHOD_REFLECT = 2,
            SVG_SPREADMETHOD_REPEAT  = 3
        };

        SVGGradientElement(const QualifiedName&, Document*);
        virtual ~SVGGradientElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);

        virtual void childrenChanged(bool changedByParser = false);
        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual SVGResource* canvasResource();

    protected:
        friend class SVGPaintServerGradient;
        friend class SVGLinearGradientElement;
        friend class SVGRadialGradientElement;

        virtual void buildGradient() const = 0;
        virtual SVGPaintServerType gradientType() const = 0;

        Vector<SVGGradientStop> buildStops() const;
        mutable RefPtr<SVGPaintServerGradient> m_resource;
 
    protected:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
 
        ANIMATED_PROPERTY_DECLARATIONS(SVGGradientElement, int, int, SpreadMethod, spreadMethod)
        ANIMATED_PROPERTY_DECLARATIONS(SVGGradientElement, int, int, GradientUnits, gradientUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGGradientElement, SVGTransformList*, RefPtr<SVGTransformList>, GradientTransform, gradientTransform)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
