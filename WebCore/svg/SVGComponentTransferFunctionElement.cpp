/*
    Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "config.h"

#if ENABLE(SVG) && ENABLE(FILTERS)
#include "SVGComponentTransferFunctionElement.h"

#include "MappedAttribute.h"
#include "SVGFEComponentTransferElement.h"
#include "SVGNames.h"
#include "SVGNumberList.h"

namespace WebCore {

char SVGComponentTransferFunctionElementIdentifier[] = "SVGComponentTransferFunctionElement";

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document* doc)
    : SVGElement(tagName, doc)
    , m_type(this, SVGNames::typeAttr, FECOMPONENTTRANSFER_TYPE_UNKNOWN)
    , m_tableValues(this, SVGNames::tableValuesAttr, SVGNumberList::create(SVGNames::tableValuesAttr))
    , m_slope(this, SVGNames::slopeAttr, 1.0f)
    , m_intercept(this, SVGNames::interceptAttr)
    , m_amplitude(this, SVGNames::amplitudeAttr, 1.0f)
    , m_exponent(this, SVGNames::exponentAttr, 1.0f)
    , m_offset(this, SVGNames::offsetAttr)
{
}

SVGComponentTransferFunctionElement::~SVGComponentTransferFunctionElement()
{
}

void SVGComponentTransferFunctionElement::parseMappedAttribute(MappedAttribute* attr)
{
    const String& value = attr->value();
    if (attr->name() == SVGNames::typeAttr)
    {
        if (value == "identity")
            setTypeBaseValue(FECOMPONENTTRANSFER_TYPE_IDENTITY);
        else if (value == "table")
            setTypeBaseValue(FECOMPONENTTRANSFER_TYPE_TABLE);
        else if (value == "discrete")
            setTypeBaseValue(FECOMPONENTTRANSFER_TYPE_DISCRETE);
        else if (value == "linear")
            setTypeBaseValue(FECOMPONENTTRANSFER_TYPE_LINEAR);
        else if (value == "gamma")
            setTypeBaseValue(FECOMPONENTTRANSFER_TYPE_GAMMA);
    }
    else if (attr->name() == SVGNames::tableValuesAttr)
        tableValuesBaseValue()->parse(value);
    else if (attr->name() == SVGNames::slopeAttr)
        setSlopeBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::interceptAttr)
        setInterceptBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::amplitudeAttr)
        setAmplitudeBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::exponentAttr)
        setExponentBaseValue(value.toFloat());
    else if (attr->name() == SVGNames::offsetAttr)
        setOffsetBaseValue(value.toFloat());
    else
        SVGElement::parseMappedAttribute(attr);
}

ComponentTransferFunction SVGComponentTransferFunctionElement::transferFunction() const
{
    ComponentTransferFunction func;
    func.type = (ComponentTransferType) type();
    func.slope = slope();
    func.intercept = intercept();
    func.amplitude = amplitude();
    func.exponent = exponent();
    func.offset = offset();
    SVGNumberList* numbers = tableValues();

    ExceptionCode ec = 0;
    unsigned int nr = numbers->numberOfItems();
    for (unsigned int i = 0; i < nr; i++)
        func.tableValues.append(numbers->getItem(i, ec));
    return func;
}

}

// vim:ts=4:noet
#endif // ENABLE(SVG)

