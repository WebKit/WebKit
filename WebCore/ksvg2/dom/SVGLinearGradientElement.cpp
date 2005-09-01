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

#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGLinearGradientElement.h"
#include "SVGLinearGradientElementImpl.h"

#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGLinearGradientElement.lut.h"
using namespace KSVG;

/*
@begin SVGLinearGradientElement::s_hashTable 5
 x1    SVGLinearGradientElementConstants::X1    DontDelete|ReadOnly
 y1    SVGLinearGradientElementConstants::Y1    DontDelete|ReadOnly
 x2    SVGLinearGradientElementConstants::X2    DontDelete|ReadOnly
 y2    SVGLinearGradientElementConstants::Y2    DontDelete|ReadOnly
@end
*/

ValueImp *SVGLinearGradientElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE
    
    switch(token)
    {
        case SVGLinearGradientElementConstants::X1:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, x1());
        case SVGLinearGradientElementConstants::Y1:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, y1());
        case SVGLinearGradientElementConstants::X2:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, x2());
        case SVGLinearGradientElementConstants::Y2:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, y2());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGLinearGradientElementImpl *>(d))

SVGLinearGradientElement SVGLinearGradientElement::null;

SVGLinearGradientElement::SVGLinearGradientElement() : SVGGradientElement()
{
}

SVGLinearGradientElement::SVGLinearGradientElement(SVGLinearGradientElementImpl *i) : SVGGradientElement(i)
{
}

SVGLinearGradientElement::SVGLinearGradientElement(const SVGLinearGradientElement &other) : SVGGradientElement()
{
    (*this) = other;
}

SVGLinearGradientElement::SVGLinearGradientElement(const KDOM::Node &other) : SVGGradientElement()
{
    (*this) = other;
}

SVGLinearGradientElement::~SVGLinearGradientElement()
{
}

SVGLinearGradientElement &SVGLinearGradientElement::operator=(const SVGLinearGradientElement &other)
{
    SVGGradientElement::operator=(other);
    return *this;
}

SVGLinearGradientElement &SVGLinearGradientElement::operator=(const KDOM::Node &other)
{
    SVGLinearGradientElementImpl *ohandle = static_cast<SVGLinearGradientElementImpl *>(other.handle());
    if(d != ohandle)
    {
        if(!ohandle || ohandle->nodeType() != KDOM::ELEMENT_NODE)
        {
            if(d)
                d->deref();

            d = 0;
        }
        else
            SVGGradientElement::operator=(other);
    }

    return *this;
}

SVGAnimatedLength SVGLinearGradientElement::x1() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->x1());
}

SVGAnimatedLength SVGLinearGradientElement::y1() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->y1());
}

SVGAnimatedLength SVGLinearGradientElement::x2() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->x2());
}

SVGAnimatedLength SVGLinearGradientElement::y2() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->y2());
}

// vim:ts=4:noet
