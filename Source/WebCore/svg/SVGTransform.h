/*
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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

#include "DOMMatrix2DInit.h"
#include "SVGMatrix.h"
#include "SVGTransformValue.h"
#include "SVGValueProperty.h"

namespace WebCore {

class SVGTransform : public SVGValueProperty<SVGTransformValue>, public SVGPropertyOwner {
public:
    static Ref<SVGTransform> create(SVGTransformValue::SVGTransformType type)
    {
        return adoptRef(*new SVGTransform(type));
    }

    static Ref<SVGTransform> create(const AffineTransform& transform = { })
    {
        return adoptRef(*new SVGTransform(SVGTransformValue::SVG_TRANSFORM_MATRIX, transform));
    }

    static Ref<SVGTransform> create(const SVGTransformValue& value)
    {
        return adoptRef(*new SVGTransform(value.type(), value.matrix()->value(), value.angle(), value.rotationCenter()));
    }

    static Ref<SVGTransform> create(SVGTransformValue&& value)
    {
        return adoptRef(*new SVGTransform(WTFMove(value)));
    }

    template<typename T>
    static ExceptionOr<Ref<SVGTransform>> create(ExceptionOr<T>&& value)
    {
        if (value.hasException())
            return value.releaseException();
        return create(value.releaseReturnValue());
    }

    ~SVGTransform()
    {
        m_value.matrix()->detach();
    }

    Ref<SVGTransform> clone() const
    {
        return SVGTransform::create(m_value);
    }

    unsigned short type() { return m_value.type(); }
    float angle() { return m_value.angle(); }
    const Ref<SVGMatrix>& matrix() { return m_value.matrix(); }

    ExceptionOr<void> setMatrix(DOMMatrix2DInit&& matrixInit)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        AffineTransform transform;
        if (matrixInit.a)
            transform.setA(matrixInit.a.value());
        if (matrixInit.b)
            transform.setB(matrixInit.b.value());
        if (matrixInit.c)
            transform.setC(matrixInit.c.value());
        if (matrixInit.d)
            transform.setD(matrixInit.d.value());
        if (matrixInit.e)
            transform.setE(matrixInit.e.value());
        if (matrixInit.f)
            transform.setF(matrixInit.f.value());
        m_value.setMatrix(transform);
        commitChange();
        return { };
    }

    ExceptionOr<void> setTranslate(float tx, float ty)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        m_value.setTranslate(tx, ty);
        commitChange();
        return { };
    }

    ExceptionOr<void> setScale(float sx, float sy)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        m_value.setScale(sx, sy);
        commitChange();
        return { };
    }

    ExceptionOr<void> setRotate(float angle, float cx, float cy)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        m_value.setRotate(angle, cx, cy);
        commitChange();
        return { };
    }

    ExceptionOr<void> setSkewX(float angle)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        m_value.setSkewX(angle);
        commitChange();
        return { };
    }

    ExceptionOr<void> setSkewY(float angle)
    {
        if (isReadOnly())
            return Exception { ExceptionCode::NoModificationAllowedError };

        m_value.setSkewY(angle);
        commitChange();
        return { };
    }

    void attach(SVGPropertyOwner* owner, SVGPropertyAccess access) override
    {
        Base::attach(owner, access);
        // Reattach the SVGMatrix to the SVGTransformValue with the new SVGPropertyAccess.
        m_value.matrix()->reattach(this, access);
    }

    void detach() override
    {
        Base::detach();
        // Reattach the SVGMatrix to the SVGTransformValue with the SVGPropertyAccess::ReadWrite.
        m_value.matrix()->reattach(this, access());
    }

private:
    using Base = SVGValueProperty<SVGTransformValue>;

    SVGTransform(SVGTransformValue::SVGTransformType type, const AffineTransform& transform = { }, float angle = 0, const FloatPoint& rotationCenter = { })
        : Base(SVGTransformValue(type, SVGMatrix::create(this, SVGPropertyAccess::ReadWrite, transform), angle, rotationCenter))
    {
    }

    SVGTransform(SVGTransformValue&& value)
        : Base(WTFMove(value))
    {
        m_value.matrix()->attach(this, SVGPropertyAccess::ReadWrite);
    }

    SVGPropertyOwner* owner() const override { return m_owner; }

    void commitPropertyChange(SVGProperty* property) override
    {
        ASSERT_UNUSED(property, property == m_value.matrix().ptr());
        if (owner())
            owner()->commitPropertyChange(this);
        m_value.matrixDidChange();
    }

    String valueAsString() const override
    {
        return m_value.valueAsString();
    }
};

} // namespace WebCore
