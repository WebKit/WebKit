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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "JSSVGMatrix.h"

#include "AffineTransform.h"
#include "SVGException.h"

using namespace KJS;

namespace WebCore {

JSValue* JSSVGMatrix::inverse(ExecState* exec, const List&)
{
    AffineTransform& imp(impl());
    KJS::JSValue* result = toJS(exec, imp.inverse());

    if (!imp.isInvertible())
        setDOMException(exec, SVG_MATRIX_NOT_INVERTABLE);

    return result;
}

JSValue* JSSVGMatrix::rotateFromVector(ExecState* exec, const List& args)
{
    AffineTransform& imp(impl());
 
    float x = args[0]->toNumber(exec);
    float y = args[1]->toNumber(exec);

    KJS::JSValue* result = toJS(exec, imp.rotateFromVector(x, y));

    if (x == 0.0 || y == 0.0)
        setDOMException(exec, SVG_INVALID_VALUE_ERR);

    return result;
}
    
}

// vim:ts=4:noet
