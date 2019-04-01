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

#include "AffineTransform.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

// FIXME: Remove this class once SVGMatrix becomes an alias to DOMMatrix.
class SVGMatrix : public SVGPropertyTearOff<AffineTransform> {
    using Base = SVGPropertyTearOff<AffineTransform>;
    using Base::Base;

public:
    static Ref<SVGMatrix> create(SVGLegacyAnimatedProperty& animatedProperty, SVGPropertyRole role, AffineTransform& value)
    {
        return adoptRef(*new SVGMatrix(&animatedProperty, role, value));
    }

    static Ref<SVGMatrix> create(const AffineTransform& initialValue = { })
    {
        return adoptRef(*new SVGMatrix(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGMatrix>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    double a() const
    {
        return propertyReference().a();
    }

    ExceptionOr<void> setA(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setA(value);
        commitChange();

        return { };
    }

    double b() const
    {
        return propertyReference().b();
    }

    ExceptionOr<void> setB(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setB(value);
        commitChange();

        return { };
    }

    double c() const
    {
        return propertyReference().c();
    }

    ExceptionOr<void> setC(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setC(value);
        commitChange();

        return { };
    }

    double d() const
    {
        return propertyReference().d();
    }

    ExceptionOr<void> setD(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setD(value);
        commitChange();

        return { };
    }

    double e() const
    {
        return propertyReference().e();
    }

    ExceptionOr<void> setE(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setE(value);
        commitChange();

        return { };
    }

    double f() const
    {
        return propertyReference().f();
    }

    ExceptionOr<void> setF(double value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setF(value);
        commitChange();

        return { };
    }

    Ref<SVGMatrix> multiply(SVGMatrix& secondMatrix) const
    {
        auto copy = propertyReference();
        copy.multiply(secondMatrix.propertyReference());
        return SVGMatrix::create(copy);
    }

    ExceptionOr<Ref<SVGMatrix>> inverse() const
    {
        auto inverse = propertyReference().inverse();
        if (!inverse)
            return Exception { InvalidStateError, "Matrix is not invertible"_s };

        return SVGMatrix::create(*inverse);
    }

    Ref<SVGMatrix> translate(float x, float y) const
    {
        auto copy = propertyReference();
        copy.translate(x, y);
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> scale(float scaleFactor) const
    {
        auto copy = propertyReference();
        copy.scale(scaleFactor);
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> scaleNonUniform(float scaleFactorX, float scaleFactorY) const
    {
        auto copy = propertyReference();
        copy.scaleNonUniform(scaleFactorX, scaleFactorY);
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> rotate(float angle) const
    {
        auto copy = propertyReference();
        copy.rotate(angle);
        return SVGMatrix::create(copy);
    }

    ExceptionOr<Ref<SVGMatrix>> rotateFromVector(float x, float y) const
    {
        if (!x || !y)
            return Exception { TypeError };

        auto copy = propertyReference();
        copy.rotateFromVector(x, y);
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> flipX() const
    {
        auto copy = propertyReference();
        copy.flipX();
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> flipY() const
    {
        auto copy = propertyReference();
        copy.flipY();
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> skewX(float angle) const
    {
        auto copy = propertyReference();
        copy.skewX(angle);
        return SVGMatrix::create(copy);
    }

    Ref<SVGMatrix> skewY(float angle) const
    {
        auto copy = propertyReference();
        copy.skewY(angle);
        return SVGMatrix::create(copy);
    }
};

} // namespace WebCore
