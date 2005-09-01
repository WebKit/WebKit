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

#include "SVGFETurbulenceElement.h"
#include "SVGFETurbulenceElementImpl.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGAnimatedNumber.h"
#include "SVGAnimatedNumberImpl.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedEnumerationImpl.h"
#include "SVGAnimatedInteger.h"
#include "SVGAnimatedIntegerImpl.h"

#include "SVGConstants.h"
#include "SVGFETurbulenceElement.lut.h"
using namespace KSVG;

/*
@begin SVGFETurbulenceElement::s_hashTable 7
 baseFrequencyX        SVGFETurbulenceElementConstants::BaseFrequencyX    DontDelete|ReadOnly
 baseFrequencyY        SVGFETurbulenceElementConstants::BaseFrequencyY    DontDelete|ReadOnly
 numOctaves            SVGFETurbulenceElementConstants::NumOctaves        DontDelete|ReadOnly
 seed                SVGFETurbulenceElementConstants::Seed            DontDelete|ReadOnly
 stitchTiles        SVGFETurbulenceElementConstants::StitchTiles    DontDelete|ReadOnly
 type                SVGFETurbulenceElementConstants::Type            DontDelete|ReadOnly
@end
*/

ValueImp *SVGFETurbulenceElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGFETurbulenceElementConstants::BaseFrequencyX:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, baseFrequencyX());
        case SVGFETurbulenceElementConstants::BaseFrequencyY:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, baseFrequencyY());
        case SVGFETurbulenceElementConstants::NumOctaves:
            return KDOM::safe_cache<SVGAnimatedInteger>(exec, numOctaves());
        case SVGFETurbulenceElementConstants::Seed:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, seed());
        case SVGFETurbulenceElementConstants::StitchTiles:
            return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, stitchTiles());
        case SVGFETurbulenceElementConstants::Type:
            return KDOM::safe_cache<SVGAnimatedEnumeration>(exec, type());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGFETurbulenceElementImpl *>(d))

SVGFETurbulenceElement SVGFETurbulenceElement::null;

SVGFETurbulenceElement::SVGFETurbulenceElement() : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
}

SVGFETurbulenceElement::SVGFETurbulenceElement(SVGFETurbulenceElementImpl *i) : SVGElement(i), SVGFilterPrimitiveStandardAttributes(i)
{
}

SVGFETurbulenceElement::SVGFETurbulenceElement(const SVGFETurbulenceElement &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
    (*this) = other;
}

SVGFETurbulenceElement::SVGFETurbulenceElement(const KDOM::Node &other) : SVGElement(), SVGFilterPrimitiveStandardAttributes()
{
    (*this) = other;
}

SVGFETurbulenceElement::~SVGFETurbulenceElement()
{
}

SVGFETurbulenceElement &SVGFETurbulenceElement::operator=(const SVGFETurbulenceElement &other)
{
    SVGElement::operator=(other);
    SVGFilterPrimitiveStandardAttributes::operator=(other);
    return *this;
}

SVGFETurbulenceElement &SVGFETurbulenceElement::operator=(const KDOM::Node &other)
{
    SVGFETurbulenceElementImpl *ohandle = static_cast<SVGFETurbulenceElementImpl *>(other.handle());
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

SVGAnimatedNumber SVGFETurbulenceElement::baseFrequencyX() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->baseFrequencyX());
}

SVGAnimatedNumber SVGFETurbulenceElement::baseFrequencyY() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->baseFrequencyY());
}

SVGAnimatedInteger SVGFETurbulenceElement::numOctaves() const
{
    if(!d)
        return SVGAnimatedInteger::null;

    return SVGAnimatedInteger(impl->numOctaves());
}

SVGAnimatedNumber SVGFETurbulenceElement::seed() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->seed());
}

SVGAnimatedEnumeration SVGFETurbulenceElement::stitchTiles() const
{
    if(!d)
        return SVGAnimatedEnumeration::null;

    return SVGAnimatedEnumeration(impl->stitchTiles());
}

SVGAnimatedEnumeration SVGFETurbulenceElement::type() const
{
    if(!d)
        return SVGAnimatedEnumeration::null;

    return SVGAnimatedEnumeration(impl->type());
}

// vim:ts=4:noet
