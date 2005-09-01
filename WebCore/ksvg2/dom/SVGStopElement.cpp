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

#include "SVGStopElement.h"
#include "SVGStopElementImpl.h"
#include "SVGAnimatedNumber.h"

#include "SVGConstants.h"
#include "SVGStopElement.lut.h"
using namespace KSVG;

/*
@begin SVGStopElement::s_hashTable 3
 offset    SVGStopElementConstants::Offset    DontDelete|ReadOnly
@end
*/

ValueImp *SVGStopElement::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGStopElementConstants::Offset:
            return KDOM::safe_cache<SVGAnimatedNumber>(exec, offset());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(KDOM::DOMException)
    return Undefined();
}

// The qdom way...
#define impl (static_cast<SVGStopElementImpl *>(d))

SVGStopElement SVGStopElement::null;

SVGStopElement::SVGStopElement() : SVGElement(), SVGStylable()
{
}

SVGStopElement::SVGStopElement(SVGStopElementImpl *i) : SVGElement(i), SVGStylable(i)
{
}

SVGStopElement::SVGStopElement(const SVGStopElement &other) : SVGElement(), SVGStylable()
{
    (*this) = other;
}

SVGStopElement::SVGStopElement(const KDOM::Node &other) : SVGElement(), SVGStylable()
{
    (*this) = other;
}

SVGStopElement::~SVGStopElement()
{
}

SVGStopElement &SVGStopElement::operator=(const SVGStopElement &other)
{
    SVGElement::operator=(other);
    SVGStylable::operator=(other);
    return *this;
}

SVGStopElement &SVGStopElement::operator=(const KDOM::Node &other)
{
    SVGStopElementImpl *ohandle = static_cast<SVGStopElementImpl *>(other.handle());
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
            SVGStylable::operator=(ohandle);
        }
    }

    return *this;
}

SVGAnimatedNumber SVGStopElement::offset() const
{
    if(!d)
        return SVGAnimatedNumber::null;

    return SVGAnimatedNumber(impl->offset());
}

// vim:ts=4:noet
