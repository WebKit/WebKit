/*
    Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>

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

#if ENABLE(SVG)
#include "JSSVGLength.h"

#include "SVGAnimatedProperty.h"

using namespace JSC;

namespace WebCore {

JSValue JSSVGLength::value(ExecState*) const
{
    SVGLength& podImp = impl()->propertyReference();
    return jsNumber(podImp.value(impl()->contextElement()));
}

JSValue JSSVGLength::convertToSpecifiedUnits(ExecState* exec)
{
    SVGLength& podImp = impl()->propertyReference();
    podImp.convertToSpecifiedUnits(exec->argument(0).toInt32(exec), impl()->contextElement());

    impl()->commitChange();
    return jsUndefined();
}

}

#endif // ENABLE(SVG)
