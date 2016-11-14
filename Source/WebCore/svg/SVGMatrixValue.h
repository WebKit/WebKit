/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#pragma once

#include "AffineTransform.h"
#include "ExceptionOr.h"
#include "SVGException.h"

namespace WebCore {

class SVGMatrixValue final : public AffineTransform {
public:
    SVGMatrixValue() = default;

    SVGMatrixValue(const AffineTransform& other)
        : AffineTransform(other)
    {
    }

    SVGMatrixValue(double a, double b, double c, double d, double e, double f)
        : AffineTransform(a, b, c, d, e, f)
    {
    }

    SVGMatrixValue translate(double tx, double ty)
    {
        AffineTransform copy { *this };
        copy.translate(tx, ty);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue scale(double s)
    {
        AffineTransform copy { *this };
        copy.scale(s, s);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue scaleNonUniform(double sx, double sy)
    {
        AffineTransform copy { *this };
        copy.scale(sx, sy);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue rotate(double d)
    {
        AffineTransform copy { *this };
        copy.rotate(d);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue flipX()
    {
        AffineTransform copy { *this };
        copy.flipX();
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue flipY()
    {
        AffineTransform copy { *this };
        copy.flipY();
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue skewX(double angle)
    {
        AffineTransform copy { *this };
        copy.skewX(angle);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue skewY(double angle)
    {
        AffineTransform copy { *this };
        copy.skewY(angle);
        return SVGMatrixValue { copy };
    }

    SVGMatrixValue multiply(const SVGMatrixValue& other)
    {
        AffineTransform copy { *this };
        copy *= static_cast<const AffineTransform&>(other);
        return SVGMatrixValue { copy };
    }

    ExceptionOr<SVGMatrixValue> inverse() const
    {
        if (auto inverse = AffineTransform::inverse())
            return SVGMatrixValue { inverse.value() };
        
        return Exception { SVGException::SVG_MATRIX_NOT_INVERTABLE };
    }

    ExceptionOr<SVGMatrixValue> rotateFromVector(double x, double y)
    {
        if (!x || !y)
            return Exception { SVGException::SVG_INVALID_VALUE_ERR };

        AffineTransform copy { *this };
        copy.rotateFromVector(x, y);
        return SVGMatrixValue { copy };
    }

};

} // namespace WebCore
