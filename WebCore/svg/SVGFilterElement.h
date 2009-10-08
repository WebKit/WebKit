/*
    Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
    Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>

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

#ifndef SVGFilterElement_h
#define SVGFilterElement_h

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGResourceFilter.h"
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

        virtual SVGResource* canvasResource();

        void setFilterRes(unsigned long filterResX, unsigned long filterResY) const;

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual bool rendererIsNeeded(RenderStyle*) { return false; }

    protected:
        virtual const SVGElement* contextElement() const { return this; }

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::filterUnitsAttrString, int, FilterUnits, filterUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::primitiveUnitsAttrString, int, PrimitiveUnits, primitiveUnits)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::xAttrString, SVGLength, X, x)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::yAttrString, SVGLength, Y, y)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::widthAttrString, SVGLength, Width, width)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGNames::heightAttrString, SVGLength, Height, height)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGFilterResXIdentifier, long, FilterResX, filterResX)
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGNames::filterTagString, SVGFilterResYIdentifier, long, FilterResY, filterResY)

        // SVGURIReference
        ANIMATED_PROPERTY_DECLARATIONS(SVGFilterElement, SVGURIReferenceIdentifier, XLinkNames::hrefAttrString, String, Href, href)

        RefPtr<SVGResourceFilter> m_filter;
    };

} // namespace WebCore

#endif // ENABLE(SVG)
#endif
