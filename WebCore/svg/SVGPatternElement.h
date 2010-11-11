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

#ifndef SVGPatternElement_h
#define SVGPatternElement_h

#if ENABLE(SVG)
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPropertyMacros.h"
#include "SVGAnimatedTransformList.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGFitToViewBox.h"
#include "SVGLangSpace.h"
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
        static PassRefPtr<SVGPatternElement> create(const QualifiedName&, Document*);

        void collectPatternAttributes(PatternAttributes&) const;

    private:
        SVGPatternElement(const QualifiedName&, Document*);
        
        virtual bool isValid() const { return SVGTests::isValid(); }
        virtual bool needsPendingResourceHandling() const { return false; }

        virtual void parseMappedAttribute(Attribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual bool selfHasRelativeLengths() const;

        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::xAttr, SVGLength, X, x)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::yAttr, SVGLength, Y, y)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::widthAttr, SVGLength, Width, width)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::heightAttr, SVGLength, Height, height)
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGPatternElement, SVGNames::patternUnitsAttr, int, PatternUnits, patternUnits)
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGPatternElement, SVGNames::patternContentUnitsAttr, int, PatternContentUnits, patternContentUnits)
        DECLARE_ANIMATED_TRANSFORM_LIST_PROPERTY_NEW(SVGPatternElement, SVGNames::patternTransformAttr, SVGTransformList, PatternTransform, patternTransform)

        // SVGURIReference
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGPatternElement, XLinkNames::hrefAttr, String, Href, href)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGPatternElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)

        // SVGPatternElement
        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::viewBoxAttr, FloatRect, ViewBox, viewBox)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGPatternElement, SVGNames::preserveAspectRatioAttr, SVGPreserveAspectRatio, PreserveAspectRatio, preserveAspectRatio) 
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
