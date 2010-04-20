/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
    Copyright (C) Research In Motion Limited 2010. All rights reserved.

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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "RenderObject.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGLangSpace.h"
#include "SVGStyledElement.h"
#include "SVGURIReference.h"

namespace WebCore {

extern char SVGFilterResXIdentifier[];
extern char SVGFilterResYIdentifier[];

class SVGFilterElement : public SVGStyledElement,
                         public SVGURIReference,
                         public SVGLangSpace,
                         public SVGExternalResourcesRequired {
public:
    SVGFilterElement(const QualifiedName&, Document*);
    virtual ~SVGFilterElement();

    void setFilterRes(unsigned long filterResX, unsigned long filterResY) const;
    FloatRect filterBoundingBox(const FloatRect&) const;

    virtual void parseMappedAttribute(MappedAttribute*);
    virtual void synchronizeProperty(const QualifiedName&);

    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);

private:
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::filterUnitsAttr, int, FilterUnits, filterUnits)
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::primitiveUnitsAttr, int, PrimitiveUnits, primitiveUnits)
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::xAttr, SVGLength, X, x)
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::yAttr, SVGLength, Y, y)
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::widthAttr, SVGLength, Width, width)
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::heightAttr, SVGLength, Height, height)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFilterElement, SVGNames::filterResAttr, SVGFilterResXIdentifier, long, FilterResX, filterResX)
    DECLARE_ANIMATED_PROPERTY_MULTIPLE_WRAPPERS(SVGFilterElement, SVGNames::filterResAttr, SVGFilterResYIdentifier, long, FilterResY, filterResY)

    // SVGURIReference
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, XLinkNames::hrefAttr, String, Href, href)

    // SVGExternalResourcesRequired
    DECLARE_ANIMATED_PROPERTY(SVGFilterElement, SVGNames::externalResourcesRequiredAttr, bool, ExternalResourcesRequired, externalResourcesRequired)
};

}

#endif
#endif
