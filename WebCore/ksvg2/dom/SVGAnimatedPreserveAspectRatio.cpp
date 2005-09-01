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
#include "SVGElementImpl.h"
#include "SVGAnimatedPreserveAspectRatio.h"
#include "SVGAnimatedPreserveAspectRatioImpl.h"

#include "SVGConstants.h"
#include "SVGAnimatedPreserveAspectRatio.lut.h"
using namespace KSVG;

/*
@begin SVGAnimatedPreserveAspectRatio::s_hashTable 3
 baseVal    SVGAnimatedPreserveAspectRatioConstants::BaseVal    DontDelete|ReadOnly
 animVal    SVGAnimatedPreserveAspectRatioConstants::AnimVal    DontDelete|ReadOnly
@end
*/

ValueImp *SVGAnimatedPreserveAspectRatio::getValueProperty(ExecState *exec, int token) const
{
    KDOM_ENTER_SAFE

    switch(token)
    {
        case SVGAnimatedPreserveAspectRatioConstants::BaseVal:
            return KDOM::safe_cache<SVGPreserveAspectRatio>(exec, baseVal());
        case SVGAnimatedPreserveAspectRatioConstants::AnimVal:
            return KDOM::safe_cache<SVGPreserveAspectRatio>(exec, animVal());
        default:
            kdWarning() << "Unhandled token in " << k_funcinfo << " : " << token << endl;
    }

    KDOM_LEAVE_SAFE(SVGException)
    return Undefined();
}

SVGAnimatedPreserveAspectRatio SVGAnimatedPreserveAspectRatio::null;

SVGAnimatedPreserveAspectRatio::SVGAnimatedPreserveAspectRatio() : impl(0)
{
}

SVGAnimatedPreserveAspectRatio::SVGAnimatedPreserveAspectRatio(SVGAnimatedPreserveAspectRatioImpl *i) : impl(i)
{
    if(impl)
        impl->ref();
}

SVGAnimatedPreserveAspectRatio::SVGAnimatedPreserveAspectRatio(const SVGAnimatedPreserveAspectRatio &other) : impl(0)
{
    (*this) = other;
}

KSVG_IMPL_DTOR_ASSIGN_OP(SVGAnimatedPreserveAspectRatio)

SVGPreserveAspectRatio SVGAnimatedPreserveAspectRatio::baseVal() const
{
    if(!impl)
        return SVGPreserveAspectRatio::null;

    return SVGPreserveAspectRatio(impl->baseVal());
}

SVGPreserveAspectRatio SVGAnimatedPreserveAspectRatio::animVal() const
{
    if(!impl)
        return SVGPreserveAspectRatio::null;

    return SVGPreserveAspectRatio(impl->animVal());
}

// vim:ts=4:noet
