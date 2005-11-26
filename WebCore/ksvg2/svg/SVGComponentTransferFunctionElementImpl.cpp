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
#include <qstringlist.h>

#include <kdom/core/AttrImpl.h>

#include "ksvg.h"
#include "SVGNames.h"
#include "SVGHelper.h"
#include "SVGRenderStyle.h"
#include "SVGComponentTransferFunctionElementImpl.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedNumberListImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGComponentTransferFunctionElementImpl::SVGComponentTransferFunctionElementImpl(const KDOM::QualifiedName& tagName, KDOM::DocumentImpl *doc) : 
SVGElementImpl(tagName, doc)
{
}

SVGComponentTransferFunctionElementImpl::~SVGComponentTransferFunctionElementImpl()
{
}

SVGAnimatedEnumerationImpl *SVGComponentTransferFunctionElementImpl::type() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedEnumerationImpl>(m_type, dummy);
}

SVGAnimatedNumberListImpl *SVGComponentTransferFunctionElementImpl::tableValues() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberListImpl>(m_tableValues, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::slope() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_slope, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::intercept() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_intercept, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::amplitude() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_amplitude, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::exponent() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_exponent, dummy);
}

SVGAnimatedNumberImpl *SVGComponentTransferFunctionElementImpl::offset() const
{
    SVGStyledElementImpl *dummy = 0;
    return lazy_create<SVGAnimatedNumberImpl>(m_offset, dummy);
}

void SVGComponentTransferFunctionElementImpl::parseMappedAttribute(KDOM::MappedAttributeImpl *attr)
{
    KDOM::DOMString value(attr->value());
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
        tableValues()->baseVal()->parse(value.qstring());
    else if (attr->name() == SVGNames::slopeAttr)
        slope()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::interceptAttr)
        intercept()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::amplitudeAttr)
        amplitude()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::exponentAttr)
        exponent()->setBaseVal(value.qstring().toDouble());
    else if (attr->name() == SVGNames::offsetAttr)
        offset()->setBaseVal(value.qstring().toDouble());
    else
        SVGElementImpl::parseMappedAttribute(attr);
}

KCComponentTransferFunction SVGComponentTransferFunctionElementImpl::transferFunction() const
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
