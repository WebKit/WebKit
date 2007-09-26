/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)

#include "JSSVGMatrix.h"

#include "AffineTransform.h"
#include "SVGException.h"

using namespace KJS;

namespace WebCore {

JSValue* JSSVGMatrix::inverse(ExecState* exec, const List&)
{
    const AffineTransform& imp(*impl());
    KJS::JSValue* result = toJS(exec, new JSSVGPODTypeWrapper<AffineTransform>(imp.inverse()));

    if (!imp.isInvertible())
        setDOMException(exec, SVG_MATRIX_NOT_INVERTABLE);

    return result;
}

JSValue* JSSVGMatrix::rotateFromVector(ExecState* exec, const List& args)
{
    AffineTransform& imp(*impl());
 
    float x = args[0]->toFloat(exec);
    float y = args[1]->toFloat(exec);

    KJS::JSValue* result = toJS(exec, new JSSVGPODTypeWrapper<AffineTransform>(imp.rotateFromVector(x, y)));

    if (x == 0.0 || y == 0.0)
        setDOMException(exec, SVG_INVALID_VALUE_ERR);

    return result;
}
    
}

#endif // ENABLE(SVG)

// vim:ts=4:noet
