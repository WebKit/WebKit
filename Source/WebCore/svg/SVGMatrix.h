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

class SVGMatrix final : public AffineTransform {
public:
    SVGMatrix() { }
    SVGMatrix(const AffineTransform& other)
        : AffineTransform(other)
    {
    }

    SVGMatrix(double a, double b, double c, double d, double e, double f)
        : AffineTransform(a, b, c, d, e, f)
    {
    }

    SVGMatrix translate(double tx, double ty)
    {
        AffineTransform copy { *this };
        copy.translate(tx, ty);
        return SVGMatrix { copy };
    }

    SVGMatrix scale(double s)
    {
        AffineTransform copy { *this };
        copy.scale(s, s);
        return SVGMatrix { copy };
    }

    SVGMatrix scaleNonUniform(double sx, double sy)
    {
        AffineTransform copy { *this };
        copy.scale(sx, sy);
        return SVGMatrix { copy };
    }

    SVGMatrix rotate(double d)
    {
        AffineTransform copy { *this };
        copy.rotate(d);
        return SVGMatrix { copy };
    }

    SVGMatrix flipX()
    {
        AffineTransform copy { *this };
        copy.flipX();
        return SVGMatrix { copy };
    }

    SVGMatrix flipY()
    {
        AffineTransform copy { *this };
        copy.flipY();
        return SVGMatrix { copy };
    }

    SVGMatrix skewX(double angle)
    {
        AffineTransform copy { *this };
        copy.skewX(angle);
        return SVGMatrix { copy };
    }

    SVGMatrix skewY(double angle)
    {
        AffineTransform copy { *this };
        copy.skewY(angle);
        return SVGMatrix { copy };
    }

    SVGMatrix multiply(const SVGMatrix& other)
    {
        AffineTransform copy { *this };
        copy *= static_cast<const AffineTransform&>(other);
        return SVGMatrix { copy };
    }

    ExceptionOr<SVGMatrix> inverse() const
    {
        if (auto inverse = AffineTransform::inverse())
            return SVGMatrix { inverse.value() };
        
        return Exception { SVGException::SVG_MATRIX_NOT_INVERTABLE };
    }

    ExceptionOr<SVGMatrix> rotateFromVector(double x, double y)
    {
        if (!x || !y)
            return Exception { SVGException::SVG_INVALID_VALUE_ERR };

        AffineTransform copy { *this };
        copy.rotateFromVector(x, y);
        return SVGMatrix { copy };
    }

};

} // namespace WebCore
