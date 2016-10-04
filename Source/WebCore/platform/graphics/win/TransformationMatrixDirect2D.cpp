/*
 * Copyright (C) 2005-2016 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "TransformationMatrix.h"

#if PLATFORM(WIN)

#include "AffineTransform.h"
#include "FloatConversion.h"

#include <d2d1.h>

namespace WebCore {

TransformationMatrix::TransformationMatrix(const D2D1_MATRIX_3X2_F& t)
{
    setA(t._11);
    setB(t._12);
    setC(t._21);
    setD(t._22);
    setE(t._31);
    setF(t._32);
}

TransformationMatrix::operator D2D1_MATRIX_3X2_F() const
{
    return D2D1::Matrix3x2F(narrowPrecisionToFloat(a()),
        narrowPrecisionToFloat(b()),
        narrowPrecisionToFloat(c()),
        narrowPrecisionToFloat(d()),
        narrowPrecisionToFloat(e()),
        narrowPrecisionToFloat(f()));
}

AffineTransform::AffineTransform(const D2D1_MATRIX_3X2_F& t)
{
    setMatrix(t._11, t._12, t._21, t._22, t._31, t._32);
}

AffineTransform::operator D2D1_MATRIX_3X2_F() const
{
    return D2D1::Matrix3x2F(narrowPrecisionToFloat(a()),
        narrowPrecisionToFloat(b()),
        narrowPrecisionToFloat(c()),
        narrowPrecisionToFloat(d()),
        narrowPrecisionToFloat(e()),
        narrowPrecisionToFloat(f()));
}

}

#endif
