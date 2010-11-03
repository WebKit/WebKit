/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGAnimatedLength.h"
#include "SVGAnimatedPropertyMacros.h"
#include "RenderObject.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGFilterElement : public SVGStyledElement,
                         public SVGURIReference,
                         public SVGLangSpace,
                         public SVGExternalResourcesRequired {
public:
    static PassRefPtr<SVGFilterElement> create(const QualifiedName&, Document*);

    void setFilterRes(unsigned long filterResX, unsigned long filterResY);
    FloatRect filterBoundingBox(const FloatRect&) const;

private:
    SVGFilterElement(const QualifiedName&, Document*);

    virtual bool needsPendingResourceHandling() const { return false; }

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);
    virtual void synchronizeProperty(const QualifiedName&);
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

    virtual bool selfHasRelativeLengths() const;

    static const AtomicString& filterResXIdentifier();
    static const AtomicString& filterResYIdentifier();

    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFilterElement, SVGNames::filterUnitsAttr, int, FilterUnits, filterUnits)
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFilterElement, SVGNames::primitiveUnitsAttr, int, PrimitiveUnits, primitiveUnits)
    DECLARE_ANIMATED_PROPERTY_NEW(SVGFilterElement, SVGNames::xAttr, SVGLength, X, x)
    DECLARE_ANIMATED_PROPERTY_NEW(SVGFilterElement, SVGNames::yAttr, SVGLength, Y, y)
    DECLARE_ANIMATED_PROPERTY_NEW(SVGFilterElement, SVGNames::widthAttr, SVGLength, Width, width)
    DECLARE_ANIMATED_PROPERTY_NEW(SVGFilterElement, SVGNames::heightAttr, SVGLength, Height, height)

    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFilterElement, SVGNames::filterResAttr, filterResXIdentifier(), long, FilterResX, filterResX)
    DECLARE_ANIMATED_STATIC_PROPERTY_MULTIPLE_WRAPPERS_NEW(SVGFilterElement, SVGNames::filterResAttr, filterResYIdentifier(), long, FilterResY, filterResY)

    // SVGURIReference
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFilterElement, XLinkNames::hrefAttr, String, Href, href)

    // SVGExternalResourcesRequired
    DECLARE_ANIMATED_STATIC_PROPERTY_NEW(SVGFilterElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)
};

}

#endif
#endif
