/*
    Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>

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

#ifndef SVGPatternElement_h
#define SVGPatternElement_h

#if ENABLE(SVG)
#include "SVGPaintServerPattern.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGTests.h"
#include "SVGURIReference.h"


namespace WebCore {

    struct PatternAttributes;
 
    class SVGLength;
    class SVGTransformList;

    class SVGPatternElement : public SVGStyledElement,
                              public SVGURIReference,
                              public SVGTests,
                              public SVGLangSpace,
                              public SVGExternalResourcesRequired,
                              public SVGFitToViewBox {
    public:
        SVGPatternElement(const QualifiedName&, Document*);
        virtual ~SVGPatternElement();
        
        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void childrenChanged(bool changedByParser = false);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource();

    protected:
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGURIReference, String, Href, href)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGExternalResourcesRequired, bool, ExternalResourcesRequired, externalResourcesRequired)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, FloatRect, ViewBox, viewBox)
        ANIMATED_PROPERTY_FORWARD_DECLARATIONS(SVGFitToViewBox, SVGPreserveAspectRatio*, PreserveAspectRatio, preserveAspectRatio)

        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGLength, SVGLength, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, int, int, PatternUnits, patternUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, int, int, PatternContentUnits, patternContentUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGTransformList*, RefPtr<SVGTransformList>, PatternTransform, patternTransform)

        mutable RefPtr<SVGPaintServerPattern> m_resource;

        virtual const SVGElement* contextElement() const { return this; }

    private:
        friend class SVGPaintServerPattern;
        void buildPattern(const FloatRect& targetRect) const;

        PatternAttributes collectPatternProperties() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
