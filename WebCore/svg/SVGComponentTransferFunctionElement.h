/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGComponentTransferFunctionElement_h
#define SVGComponentTransferFunctionElement_h

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)
#include "SVGElement.h"
#include "SVGNumberList.h"
#include "FEComponentTransfer.h"

namespace WebCore {

    extern char SVGComponentTransferFunctionElementIdentifier[];

    class SVGComponentTransferFunctionElement : public SVGElement {
    public:
        SVGComponentTransferFunctionElement(const QualifiedName&, Document*);
        virtual ~SVGComponentTransferFunctionElement();

        virtual void parseMappedAttribute(MappedAttribute* attr);
        
        ComponentTransferFunction transferFunction() const;

    private:
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::typeAttrString, int, Type, type)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::tableValuesAttrString, SVGNumberList, TableValues, tableValues)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::slopeAttrString, float, Slope, slope)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::interceptAttrString, float, Intercept, intercept)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::amplitudeAttrString, float, Amplitude, amplitude)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::exponentAttrString, float, Exponent, exponent)
        ANIMATED_PROPERTY_DECLARATIONS(SVGComponentTransferFunctionElement, SVGComponentTransferFunctionElementIdentifier, SVGNames::offsetAttrString, float, Offset, offset)
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
#endif
