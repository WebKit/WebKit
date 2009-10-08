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
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
#include "SVGPaintServerPattern.h"
#include "SVGStyledElement.h"
#include "SVGTests.h"
#include "SVGTransformList.h"
#include "SVGURIReference.h"

namespace WebCore {

    struct PatternAttributes;
 
    class SVGLength;

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
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
        virtual SVGResource* canvasResource();

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::xAttrString, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::yAttrString, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::widthAttrString, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::heightAttrString, SVGLength, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::patternUnitsAttrString, int, PatternUnits, patternUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::patternContentUnitsAttrString, int, PatternContentUnits, patternContentUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGNames::patternTagString, SVGNames::patternTransformAttrString, SVGTransformList, PatternTransform, patternTransform)

        // SVGURIReference
        ANIMATED_PROPERTY_DECLARATIONS(SVGPatternElement, SVGURIReferenceIdentifier, XLinkNames::hrefAttrString, String, Href, href)

        // SVGExternalResourcesRequired
        ANIMATED_PROPERTY_DECLARATIONS(SVGExternalResourcesRequired, SVGExternalResourcesRequiredIdentifier,
                                       SVGNames::externalResourcesRequiredAttrString, bool,
                                       ExternalResourcesRequired, externalResourcesRequired)

        mutable RefPtr<SVGPaintServerPattern> m_resource;

    private:
        friend class SVGPaintServerPattern;
        void buildPattern(const FloatRect& targetRect) const;

        PatternAttributes collectPatternProperties() const;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
