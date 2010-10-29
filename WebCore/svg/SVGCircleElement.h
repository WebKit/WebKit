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

#ifndef SVGCircleElement_h
#define SVGCircleElement_h

#if ENABLE(SVG)
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPropertyMacros.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledTransformableElement.h"
#include "SVGTests.h"

namespace WebCore {

    class SVGCircleElement : public SVGStyledTransformableElement,
                             public SVGTests,
                             public SVGLangSpace,
                             public SVGExternalResourcesRequired {
    public:
        static PassRefPtr<SVGCircleElement> create(const QualifiedName&, Document*);

    private:
        SVGCircleElement(const QualifiedName&, Document*);

        virtual bool isValid() const { return SVGTests::isValid(); }

        virtual void parseMappedAttribute(Attribute*);
        virtual void svgAttributeChanged(const QualifiedName&);
        virtual void synchronizeProperty(const QualifiedName&);

        virtual void toPathData(Path&) const;

        virtual bool selfHasRelativeLengths() const;

        DECLARE_ANIMATED_PROPERTY_NEW(SVGCircleElement, SVGNames::cxAttr, SVGLength, Cx, cx)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGCircleElement, SVGNames::cyAttr, SVGLength, Cy, cy)
        DECLARE_ANIMATED_PROPERTY_NEW(SVGCircleElement, SVGNames::rAttr, SVGLength, R, r)

        // SVGExternalResourcesRequired
        DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGCircleElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGCircleElement_h
