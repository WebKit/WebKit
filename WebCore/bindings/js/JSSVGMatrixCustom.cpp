/*
 * Copyright (C) 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "TransformationMatrix.h"
#include "SVGException.h"

using namespace JSC;

namespace WebCore {

JSValue* JSSVGMatrix::multiply(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    TransformationMatrix secondMatrix = toSVGMatrix(args.at(exec, 0));    
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.multiply(secondMatrix)).get(), m_context.get());
}

JSValue* JSSVGMatrix::inverse(ExecState* exec, const ArgList&)
{
    TransformationMatrix imp(*impl());
    JSC::JSValue* result = toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.inverse()).get(), m_context.get());

    if (!imp.isInvertible())
        setDOMException(exec, SVGException::SVG_MATRIX_NOT_INVERTABLE);

    return result;
}

JSValue* JSSVGMatrix::translate(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float x = args.at(exec, 0)->toFloat(exec);
    float y = args.at(exec, 1)->toFloat(exec);

    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.translate(x, y)).get(), m_context.get());
}

JSValue* JSSVGMatrix::scale(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float scaleFactor = args.at(exec, 0)->toFloat(exec);
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.scale(scaleFactor)).get(), m_context.get());
}

JSValue* JSSVGMatrix::scaleNonUniform(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float scaleFactorX = args.at(exec, 0)->toFloat(exec);
    float scaleFactorY = args.at(exec, 1)->toFloat(exec);

    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.scaleNonUniform(scaleFactorX, scaleFactorY)).get(), m_context.get());
}

JSValue* JSSVGMatrix::rotate(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float angle = args.at(exec, 0)->toFloat(exec);
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.rotate(angle)).get(), m_context.get());
}

JSValue* JSSVGMatrix::rotateFromVector(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());
 
    float x = args.at(exec, 0)->toFloat(exec);
    float y = args.at(exec, 1)->toFloat(exec);

    JSC::JSValue* result = toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.rotateFromVector(x, y)).get(), m_context.get());

    if (x == 0.0 || y == 0.0)
        setDOMException(exec, SVGException::SVG_INVALID_VALUE_ERR);

    return result;
}

JSValue* JSSVGMatrix::flipX(ExecState* exec, const ArgList&)
{
    TransformationMatrix imp(*impl());
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.flipX()).get(), m_context.get());
}

JSValue* JSSVGMatrix::flipY(ExecState* exec, const ArgList&)
{
    TransformationMatrix imp(*impl());
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.flipY()).get(), m_context.get());
}

JSValue* JSSVGMatrix::skewX(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float angle = args.at(exec, 0)->toFloat(exec);
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.skewX(angle)).get(), m_context.get());
}

JSValue* JSSVGMatrix::skewY(ExecState* exec, const ArgList& args)
{
    TransformationMatrix imp(*impl());

    float angle = args.at(exec, 0)->toFloat(exec);
    return toJS(exec, JSSVGStaticPODTypeWrapper<TransformationMatrix>::create(imp.skewY(angle)).get(), m_context.get());
}

}

#endif // ENABLE(SVG)
