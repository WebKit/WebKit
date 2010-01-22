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

using namespace JSC;

namespace WebCore {

JSValue JSSVGLength::value(ExecState* exec) const
{
    JSSVGPODTypeWrapper<SVGLength>* imp = impl();
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(const_cast<JSSVGLength*>(this));

    SVGLength podImp(*imp);
    return jsNumber(exec, podImp.value(context));
}

JSValue JSSVGLength::convertToSpecifiedUnits(ExecState* exec, const ArgList& args)
{
    JSSVGPODTypeWrapper<SVGLength>* imp = impl();
    SVGElement* context = JSSVGContextCache::svgContextForDOMObject(this);

    SVGLength podImp(*imp);
    podImp.convertToSpecifiedUnits(args.at(0).toInt32(exec), context);

    imp->commitChange(podImp, this);
    return jsUndefined();
}

}

#endif // ENABLE(SVG)
