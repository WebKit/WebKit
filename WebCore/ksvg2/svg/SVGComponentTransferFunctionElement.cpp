/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include <DeprecatedStringList.h>

#include <kdom/core/Attr.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGComponentTransferFunctionElement.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedNumberList.h"
#include "SVGDOMImplementation.h"

using namespace WebCore;

SVGComponentTransferFunctionElement::SVGComponentTransferFunctionElement(const QualifiedName& tagName, Document *doc) : 
SVGElement(tagName, doc)
{
}

SVGComponentTransferFunctionElement::~SVGComponentTransferFunctionElement()
{
}

SVGAnimatedEnumeration *SVGComponentTransferFunctionElement::type() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedEnumeration>(m_type, dummy);
}

SVGAnimatedNumberList *SVGComponentTransferFunctionElement::tableValues() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumberList>(m_tableValues, dummy);
}

SVGAnimatedNumber *SVGComponentTransferFunctionElement::slope() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_slope, dummy);
}

SVGAnimatedNumber *SVGComponentTransferFunctionElement::intercept() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_intercept, dummy);
}

SVGAnimatedNumber *SVGComponentTransferFunctionElement::amplitude() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_amplitude, dummy);
}

SVGAnimatedNumber *SVGComponentTransferFunctionElement::exponent() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_exponent, dummy);
}

SVGAnimatedNumber *SVGComponentTransferFunctionElement::offset() const
{
    SVGStyledElement *dummy = 0;
    return lazy_create<SVGAnimatedNumber>(m_offset, dummy);
}

void SVGComponentTransferFunctionElement::parseMappedAttribute(MappedAttribute *attr)
{
    String value(attr->value());
    if (attr->name() == SVGNames::typeAttr)
    {
        if(value == "identity")
            type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY);
        else if(value == "table")
            type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_TABLE);
        else if(value == "discrete")
            type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE);
        else if(value == "linear")
            type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_LINEAR);
        else if(value == "gamma")
            type()->setBaseVal(SVG_FECOMPONENTTRANSFER_TYPE_GAMMA);
    }
    else if (attr->name() == SVGNames::valuesAttr)
        tableValues()->baseVal()->parse(value.deprecatedString());
    else if (attr->name() == SVGNames::slopeAttr)
        slope()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::interceptAttr)
        intercept()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::amplitudeAttr)
        amplitude()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::exponentAttr)
        exponent()->setBaseVal(value.deprecatedString().toDouble());
    else if (attr->name() == SVGNames::offsetAttr)
        offset()->setBaseVal(value.deprecatedString().toDouble());
    else
        SVGElement::parseMappedAttribute(attr);
}

KCComponentTransferFunction SVGComponentTransferFunctionElement::transferFunction() const
{
    KCComponentTransferFunction func;
    func.type = (KCComponentTransferType)(type()->baseVal() - 1);
    func.slope = slope()->baseVal();
    func.intercept = intercept()->baseVal();
    func.amplitude = amplitude()->baseVal();
    func.exponent = exponent()->baseVal();
    func.offset = offset()->baseVal();
    return func;
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

