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
#include "SVGRadialGradientElement.h"
#include "SVGRadialGradientElementImpl.h"

#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGRadialGradientElement.lut.h"
using namespace KSVG;

/*
@begin SVGRadialGradientElement::s_hashTable 7
 cx    SVGRadialGradientElementConstants::Cx    DontDelete|ReadOnly
 cy    SVGRadialGradientElementConstants::Cy    DontDelete|ReadOnly
 fx    SVGRadialGradientElementConstants::Fx    DontDelete|ReadOnly
 fy    SVGRadialGradientElementConstants::Fy    DontDelete|ReadOnly
 r    SVGRadialGradientElementConstants::R    DontDelete|ReadOnly
@end
*/

ValueImp *SVGRadialGradientElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGRadialGradientElementConstants::Cx:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, cx());
        case SVGRadialGradientElementConstants::Cy:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, cy());
        case SVGRadialGradientElementConstants::Fx:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, fx());
        case SVGRadialGradientElementConstants::Fy:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, fy());
        case SVGRadialGradientElementConstants::R:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, r());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGRadialGradientElementImpl *>(d))

SVGRadialGradientElement SVGRadialGradientElement::null;

SVGRadialGradientElement::SVGRadialGradientElement() : SVGGradientElement()
{
}

SVGRadialGradientElement::SVGRadialGradientElement(SVGRadialGradientElementImpl *i) : SVGGradientElement(i)
{
}

SVGRadialGradientElement::SVGRadialGradientElement(const SVGRadialGradientElement &other) : SVGGradientElement()
{
    (*this) = other;
}

SVGRadialGradientElement::SVGRadialGradientElement(const KDOM::Node &other) : SVGGradientElement()
{
    (*this) = other;
}

SVGRadialGradientElement::~SVGRadialGradientElement()
{
}

SVGRadialGradientElement &SVGRadialGradientElement::operator=(const SVGRadialGradientElement &other)
{
    SVGGradientElement::operator=(other);
    return *this;
}

SVGRadialGradientElement &SVGRadialGradientElement::operator=(const KDOM::Node &other)
{
    SVGRadialGradientElementImpl *ohandle = static_cast<SVGRadialGradientElementImpl *>(other.handle());
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

SVGAnimatedLength SVGRadialGradientElement::cx() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->cx());
}

SVGAnimatedLength SVGRadialGradientElement::cy() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->cy());
}

SVGAnimatedLength SVGRadialGradientElement::fx() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->fx());
}

SVGAnimatedLength SVGRadialGradientElement::fy() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->fy());
}

SVGAnimatedLength SVGRadialGradientElement::r() const
{
    if(!d)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->r());
}

// vim:ts=4:noet
