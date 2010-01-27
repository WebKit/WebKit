/*
    Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
                  2004, 2005, 2006 Rob Buis <buis@kde.org>

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

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGNumberList.h"
#include "FEComponentTransfer.h"

namespace WebCore {

    class SVGComponentTransferFunctionElement : public SVGElement {
    public:
        SVGComponentTransferFunctionElement(const QualifiedName&, Document*);
        virtual ~SVGComponentTransferFunctionElement();

        virtual void parseMappedAttribute(MappedAttribute*);
        virtual void synchronizeProperty(const QualifiedName&);
        
        ComponentTransferFunction transferFunction() const;

    private:
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::typeAttr, int, Type, type)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::tableValuesAttr, SVGNumberList*, TableValues, tableValues)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::slopeAttr, float, Slope, slope)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::interceptAttr, float, Intercept, intercept)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::amplitudeAttr, float, Amplitude, amplitude)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::exponentAttr, float, Exponent, exponent)
        DECLARE_ANIMATED_PROPERTY(SVGComponentTransferFunctionElement, SVGNames::offsetAttr, float, Offset, offset)
    };

} // namespace WebCore

#endif // ENABLE(SVG) && ENABLE(FILTERS)
#endif
