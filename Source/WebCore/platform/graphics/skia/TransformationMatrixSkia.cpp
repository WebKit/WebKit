/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)
#include "AffineTransform.h"
#include <skia/core/SkM44.h>

namespace WebCore {

TransformationMatrix::TransformationMatrix(const SkM44& t)
    : TransformationMatrix(SkScalarToDouble(t.rc(0, 0)), SkScalarToDouble(t.rc(0, 1)), SkScalarToDouble(t.rc(0, 2)), SkScalarToDouble(t.rc(0, 3)),
        SkScalarToDouble(t.rc(1, 0)), SkScalarToDouble(t.rc(1, 1)), SkScalarToDouble(t.rc(1, 2)), SkScalarToDouble(t.rc(1, 3)),
        SkScalarToDouble(t.rc(2, 0)), SkScalarToDouble(t.rc(2, 1)), SkScalarToDouble(t.rc(2, 2)), SkScalarToDouble(t.rc(2, 3)),
        SkScalarToDouble(t.rc(3, 0)), SkScalarToDouble(t.rc(3, 1)), SkScalarToDouble(t.rc(3, 2)), SkScalarToDouble(t.rc(3, 3)))
{
}

TransformationMatrix::operator SkM44() const
{
    return SkM44 {
        SkDoubleToScalar(m11()),
        SkDoubleToScalar(m12()),
        SkDoubleToScalar(m13()),
        SkDoubleToScalar(m14()),
        SkDoubleToScalar(m21()),
        SkDoubleToScalar(m22()),
        SkDoubleToScalar(m23()),
        SkDoubleToScalar(m24()),
        SkDoubleToScalar(m31()),
        SkDoubleToScalar(m32()),
        SkDoubleToScalar(m33()),
        SkDoubleToScalar(m34()),
        SkDoubleToScalar(m41()),
        SkDoubleToScalar(m42()),
        SkDoubleToScalar(m43()),
        SkDoubleToScalar(m44())
    };
}

AffineTransform::AffineTransform(const SkMatrix& t)
    : AffineTransform(SkScalarToDouble(t[SkMatrix::kMScaleX]), SkScalarToDouble(t[SkMatrix::kMSkewY]), SkScalarToDouble(t[SkMatrix::kMSkewX]),  SkScalarToDouble(t[SkMatrix::kMScaleY]),
        SkScalarToDouble(t[SkMatrix::kMTransX]), SkScalarToDouble(t[SkMatrix::kMTransY]))
{
}

AffineTransform::operator SkMatrix() const
{
    return SkMatrix::MakeAll(SkDoubleToScalar(a()), SkDoubleToScalar(c()), SkDoubleToScalar(e()), SkDoubleToScalar(b()), SkDoubleToScalar(d()), SkDoubleToScalar(f()), 0, 0, SK_Scalar1);
}

} // namespace WebCore

#endif // USE(SKIA)
