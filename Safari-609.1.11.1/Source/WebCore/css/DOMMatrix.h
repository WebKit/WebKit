/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DOMMatrixReadOnly.h"

namespace WebCore {

class ScriptExecutionContext;

class DOMMatrix : public DOMMatrixReadOnly {
public:
    static ExceptionOr<Ref<DOMMatrix>> create(ScriptExecutionContext&, Optional<Variant<String, Vector<double>>>&&);

    static Ref<DOMMatrix> create(const TransformationMatrix& matrix, Is2D is2D)
    {
        return adoptRef(*new DOMMatrix(matrix, is2D));
    }

    static Ref<DOMMatrix> create(TransformationMatrix&& matrix, Is2D is2D)
    {
        return adoptRef(*new DOMMatrix(WTFMove(matrix), is2D));
    }

    static ExceptionOr<Ref<DOMMatrix>> fromMatrix(DOMMatrixInit&&);

    static ExceptionOr<Ref<DOMMatrix>> fromFloat32Array(Ref<Float32Array>&&);
    static ExceptionOr<Ref<DOMMatrix>> fromFloat64Array(Ref<Float64Array>&&);

    ExceptionOr<Ref<DOMMatrix>> multiplySelf(DOMMatrixInit&& other);
    ExceptionOr<Ref<DOMMatrix>> preMultiplySelf(DOMMatrixInit&& other);
    Ref<DOMMatrix> translateSelf(double tx = 0, double ty = 0, double tz = 0);
    Ref<DOMMatrix> scaleSelf(double scaleX = 1, Optional<double> scaleY = WTF::nullopt, double scaleZ = 1, double originX = 0, double originY = 0, double originZ = 0);
    Ref<DOMMatrix> scale3dSelf(double scale = 1, double originX = 0, double originY = 0, double originZ = 0);
    Ref<DOMMatrix> rotateSelf(double rotX = 0, Optional<double> rotY = WTF::nullopt, Optional<double> rotZ = WTF::nullopt); // Angles are in degrees.
    Ref<DOMMatrix> rotateFromVectorSelf(double x = 0, double y = 0);
    Ref<DOMMatrix> rotateAxisAngleSelf(double x = 0, double y = 0, double z = 0, double angle = 0); // Angle is in degrees.
    Ref<DOMMatrix> skewXSelf(double sx = 0); // Angle is in degrees.
    Ref<DOMMatrix> skewYSelf(double sy = 0); // Angle is in degrees.
    Ref<DOMMatrix> invertSelf();

    ExceptionOr<Ref<DOMMatrix>> setMatrixValueForBindings(const String&);

    void setA(double f) { m_matrix.setA(f); }
    void setB(double f) { m_matrix.setB(f); }
    void setC(double f) { m_matrix.setC(f); }
    void setD(double f) { m_matrix.setD(f); }
    void setE(double f) { m_matrix.setE(f); }
    void setF(double f) { m_matrix.setF(f); }

    void setM11(double f) { m_matrix.setM11(f); }
    void setM12(double f) { m_matrix.setM12(f); }
    void setM13(double f);
    void setM14(double f);
    void setM21(double f) { m_matrix.setM21(f); }
    void setM22(double f) { m_matrix.setM22(f); }
    void setM23(double f);
    void setM24(double f);
    void setM31(double f);
    void setM32(double f);
    void setM33(double f);
    void setM34(double f);
    void setM41(double f) { m_matrix.setM41(f); }
    void setM42(double f) { m_matrix.setM42(f); }
    void setM43(double f);
    void setM44(double f);

private:
    DOMMatrix() = default;
    DOMMatrix(const TransformationMatrix&, Is2D);
    DOMMatrix(TransformationMatrix&&, Is2D);
};
static_assert(sizeof(DOMMatrix) == sizeof(DOMMatrixReadOnly), "");

inline void DOMMatrix::setM13(double f)
{
    m_matrix.setM13(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM14(double f)
{
    m_matrix.setM14(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM23(double f)
{
    m_matrix.setM23(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM24(double f)
{
    m_matrix.setM24(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM31(double f)
{
    m_matrix.setM31(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM32(double f)
{
    m_matrix.setM32(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM33(double f)
{
    m_matrix.setM33(f);
    if (f != 1)
        m_is2D = false;
}

inline void DOMMatrix::setM34(double f)
{
    m_matrix.setM34(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM43(double f)
{
    m_matrix.setM43(f);
    if (f)
        m_is2D = false;
}

inline void DOMMatrix::setM44(double f)
{
    m_matrix.setM44(f);
    if (f != 1)
        m_is2D = false;
}

} // namespace WebCore
