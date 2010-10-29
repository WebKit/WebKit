/*
 * Copyright (C) 2005 Alexander Kellett <lypanov@kde.org>
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

#ifndef SVGMaskElement_h
#define SVGMaskElement_h

#if ENABLE(SVG)
#include "RenderObject.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPropertyMacros.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledLocatableElement.h"
#include "SVGTests.h"

namespace WebCore {

    class SVGLength;

    class SVGMaskElement : public SVGStyledLocatableElement,
                           public SVGTests,
                           public SVGLangSpace,
                           public SVGExternalResourcesRequired {
    public:
        static PassRefPtr<SVGMaskElement> create(const QualifiedName&, Document*);

        FloatRect maskBoundingBox(const FloatRect&) const;

    private:
        SVGMaskElement(const QualifiedName&, Document*);

        virtual bool isValid() const { return SVGTests::isValid(); }
        virtual bool needsPendingResourceHandling() const { return false; }

        virtual void parseMappedAttribute(Attribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);
        virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

        virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

        virtual bool selfHasRelativeLengths() const;

        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGMaskElement, SVGNames::maskUnitsAttr, int, MaskUnits, maskUnits)
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGMaskElement, SVGNames::maskContentUnitsAttr, int, MaskContentUnits, maskContentUnits)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGMaskElement, SVGNames::xAttr, SVGLength, X, x)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGMaskElement, SVGNames::yAttr, SVGLength, Y, y)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGMaskElement, SVGNames::widthAttr, SVGLength, Width, width)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGMaskElement, SVGNames::heightAttr, SVGLength, Height, height)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGMaskElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)
    };

}

#endif
#endif
