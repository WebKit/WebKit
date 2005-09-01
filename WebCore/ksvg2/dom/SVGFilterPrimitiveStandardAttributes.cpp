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

#include "SVGElement.h"
#include "SVGFilterPrimitiveStandardAttributes.h"
#include "SVGException.h"
#include "SVGExceptionImpl.h"
#include "SVGFilterPrimitiveStandardAttributesImpl.h"
#include "SVGAnimatedString.h"
#include "SVGAnimatedStringImpl.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedLengthImpl.h"

#include "SVGConstants.h"
#include "SVGFilterPrimitiveStandardAttributes.lut.h"
using namespace KSVG;

/*
@begin SVGFilterPrimitiveStandardAttributes::s_hashTable 7
 x            SVGFilterPrimitiveStandardAttributesConstants::X        DontDelete|ReadOnly
 y            SVGFilterPrimitiveStandardAttributesConstants::Y        DontDelete|ReadOnly
 width        SVGFilterPrimitiveStandardAttributesConstants::Width    DontDelete|ReadOnly
 height        SVGFilterPrimitiveStandardAttributesConstants::Height    DontDelete|ReadOnly
 result        SVGFilterPrimitiveStandardAttributesConstants::Result    DontDelete|ReadOnly
@end
*/

ValueImp *SVGFilterPrimitiveStandardAttributes::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGFilterPrimitiveStandardAttributesConstants::X:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, x());
        case SVGFilterPrimitiveStandardAttributesConstants::Y:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, y());
        case SVGFilterPrimitiveStandardAttributesConstants::Width:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, width());
        case SVGFilterPrimitiveStandardAttributesConstants::Height:
            return KDOM::safe_cache<SVGAnimatedLength>(exec, height());
        case SVGFilterPrimitiveStandardAttributesConstants::Result:
            return KDOM::safe_cache<SVGAnimatedString>(exec, result());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

SVGFilterPrimitiveStandardAttributes SVGFilterPrimitiveStandardAttributes::null;

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes() : SVGStylable(), impl(0)
{
}

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(SVGFilterPrimitiveStandardAttributesImpl *i) : SVGStylable(i), impl(i)
{
}

SVGFilterPrimitiveStandardAttributes::SVGFilterPrimitiveStandardAttributes(const SVGFilterPrimitiveStandardAttributes &other) : SVGStylable(), impl(0)
{
    (*this) = other;
}

SVGFilterPrimitiveStandardAttributes::~SVGFilterPrimitiveStandardAttributes()
{
}

SVGFilterPrimitiveStandardAttributes &SVGFilterPrimitiveStandardAttributes::operator=(const SVGFilterPrimitiveStandardAttributes &other)
{
    SVGStylable::operator=(other);
    if(impl != other.impl)
        impl = other.impl;

    return *this;
}

SVGFilterPrimitiveStandardAttributes &SVGFilterPrimitiveStandardAttributes::operator=(SVGFilterPrimitiveStandardAttributesImpl *other)
{
    SVGStylable::operator=(other);
    if(impl != other)
        impl = other;

    return *this;
}

SVGAnimatedLength SVGFilterPrimitiveStandardAttributes::x() const
{
    if(!impl)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->x());
}

SVGAnimatedLength SVGFilterPrimitiveStandardAttributes::y() const
{
    if(!impl)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->y());
}

SVGAnimatedLength SVGFilterPrimitiveStandardAttributes::width() const
{
    if(!impl)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->width());
}

SVGAnimatedLength SVGFilterPrimitiveStandardAttributes::height() const
{
    if(!impl)
        return SVGAnimatedLength::null;

    return SVGAnimatedLength(impl->height());
}

SVGAnimatedString SVGFilterPrimitiveStandardAttributes::result() const
{
    if(!impl)
        return SVGAnimatedString::null;

    return SVGAnimatedString(impl->result());
}

// vim:ts=4:noet
