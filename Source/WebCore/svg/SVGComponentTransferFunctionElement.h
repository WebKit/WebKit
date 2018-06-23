/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#pragma once

#include "FEComponentTransfer.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberList.h"
#include "SVGElement.h"

namespace WebCore {

template<>
struct SVGPropertyTraits<ComponentTransferType> {
    static unsigned highestEnumValue() { return FECOMPONENTTRANSFER_TYPE_GAMMA; }

    static String toString(ComponentTransferType type)
    {
        switch (type) {
        case FECOMPONENTTRANSFER_TYPE_UNKNOWN:
            return emptyString();
        case FECOMPONENTTRANSFER_TYPE_IDENTITY:
            return "identity"_s;
        case FECOMPONENTTRANSFER_TYPE_TABLE:
            return "table"_s;
        case FECOMPONENTTRANSFER_TYPE_DISCRETE:
            return "discrete"_s;
        case FECOMPONENTTRANSFER_TYPE_LINEAR:
            return "linear"_s;
        case FECOMPONENTTRANSFER_TYPE_GAMMA:
            return "gamma"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static ComponentTransferType fromString(const String& value)
    {
        if (value == "identity")
            return FECOMPONENTTRANSFER_TYPE_IDENTITY;
        if (value == "table")
            return FECOMPONENTTRANSFER_TYPE_TABLE;
        if (value == "discrete")
            return FECOMPONENTTRANSFER_TYPE_DISCRETE;
        if (value == "linear")
            return FECOMPONENTTRANSFER_TYPE_LINEAR;
        if (value == "gamma")
            return FECOMPONENTTRANSFER_TYPE_GAMMA;
        return FECOMPONENTTRANSFER_TYPE_UNKNOWN;
    }
};

class SVGComponentTransferFunctionElement : public SVGElement {
    WTF_MAKE_ISO_ALLOCATED(SVGComponentTransferFunctionElement);
public:
    ComponentTransferFunction transferFunction() const;

protected:
    SVGComponentTransferFunctionElement(const QualifiedName&, Document&);

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool rendererIsNeeded(const RenderStyle&) override { return false; }
    
private:
    static bool isSupportedAttribute(const QualifiedName&);

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGComponentTransferFunctionElement)
        DECLARE_ANIMATED_ENUMERATION(Type, type, ComponentTransferType)
        DECLARE_ANIMATED_NUMBER_LIST(TableValues, tableValues)
        DECLARE_ANIMATED_NUMBER(Slope, slope)
        DECLARE_ANIMATED_NUMBER(Intercept, intercept)
        DECLARE_ANIMATED_NUMBER(Amplitude, amplitude)
        DECLARE_ANIMATED_NUMBER(Exponent, exponent)
        DECLARE_ANIMATED_NUMBER(Offset, offset)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore
