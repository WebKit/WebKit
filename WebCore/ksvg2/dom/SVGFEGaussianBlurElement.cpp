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

#include <kdom/ecma/Ecma.h>

#include <kdom/DOMException.h>
#include <kdom/impl/DOMExceptionImpl.h>

#include "SVGFEGaussianBlurElement.h"
#include "SVGFEGaussianBlurElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"

#include "SVGConstants.h"
#include "SVGFEGaussianBlurElement.lut.h"
using namespace KSVG;

/*
@begin SVGFEGaussianBlurElement::s_hashTable 5
 in1            SVGFEGaussianBlurElementConstants::In1                DontDelete|ReadOnly
 stdDeviationX    SVGFEGaussianBlurElementConstants::StdDeviationX    DontDelete|ReadOnly
 stdDeviationY    SVGFEGaussianBlurElementConstants::StdDeviationY    DontDelete|ReadOnly
@end
@begin SVGFEGaussianBlurElementProto::s_hashTable 3
 setStdDeviation    SVGFEGaussianBlurElementConstants::SetStdDeviation    DontDelete|Function 2
@end
*/

KSVG_IMPLEMENT_PROTOTYPE("SVGFEGaussianBlurElement", SVGFEGaussianBlurElementProto, SVGFEGaussianBlurElementProtoFunc)

ValueImp *SVGFEGaussianBlurElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGFEGaussianBlurElementConstants::In1:
            return KDOM::safe_cache<SVGAnimatedString>(exec, in1());
        case SVGFEGaussianBlurElementConstants::StdDeviationX:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, stdDeviationX());
        case SVGFEGaussianBlurElementConstants::StdDeviationY:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, stdDeviationY());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
    return Undefined();
}

ValueImp *SVGFEGaussianBlurElementProtoFunc::callAsFunction(ExecState *exec, ObjectImp *thisObj, const List &args)
{
    SVGFEGaussianBlurElement obj(cast(exec, thisObj));
    KDOM_ENTER_SAFE

    switch(id)
    {
        case SVGFEGaussianBlurElementConstants::SetStdDeviation:
        {
            float stdDeviationX = args[0]->toNumber(exec);
            float stdDeviationY = args[1]->toNumber(exec);
            obj.setStdDeviation(stdDeviationX, stdDeviationY);

            return Undefined();
        }
        default:
            kdWarning() << "Unhandled function id in " << k_funcinfo << " : " << id << endl;
    }

    KDOM_LEAVE_CALL_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFEGaussianBlurElementImpl *>(d))

SVGFEGaussianBlurElement SVGFEGaussianBlurElement::null;

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(SVGFEGaussianBlurElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const SVGFEGaussianBlurElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
    (*this) = other;
}

SVGFEGaussianBlurElement::SVGFEGaussianBlurElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
    (*this) = other;
}

SVGFEGaussianBlurElement::~SVGFEGaussianBlurElement()
{
}

SVGFEGaussianBlurElement &SVGFEGaussianBlurElement::operator=(const SVGFEGaussianBlurElement &other)
{
    SVGElement::operator=(other);
    SVGFilterPrimitiveStandardAttributes::operator=(other);
    return *this;
}

SVGFEGaussianBlurElement &SVGFEGaussianBlurElement::operator=(const KDOM::Node &other)
{
    SVGFEGaussianBlurElementImpl *ohandle = static_cast<SVGFEGaussianBlurElementImpl *>(other.handle());
    if(d != ohandle)
    {
        if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
        {
            if(d)
                d->deref();
            
            d = 0;
        }
        else
        {
            SVGElement::operator=(other);
            SVGFilterPrimitiveStandardAttributes::operator=(ohandle);
        }
    }

    return *this;
}

SVGAnimatedString SVGFEGaussianBlurElement::in1() const
{
    if(!d)
        return SVGAnimatedString::null;

    return SVGAnimatedString(impl->in1());
}

SVGAnimatedNumber SVGFEGaussianBlurElement::stdDeviationX() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->stdDeviationX());
}

SVGAnimatedNumber SVGFEGaussianBlurElement::stdDeviationY() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->stdDeviationY());
}

void SVGFEGaussianBlurElement::setStdDeviation(float stdDeviationX, float stdDeviationY)
{
    if(d)
        impl->setStdDeviation(stdDeviationX, stdDeviationY);
}

// vim:ts=4:noet
