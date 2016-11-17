/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "ExceptionCode.h"
#include "SVGMatrixValue.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGMatrix : public SVGPropertyTearOff<SVGMatrixValue> {
public:
    static Ref<SVGMatrix> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGMatrixValue& value)
    {
        return adoptRef(*new SVGMatrix(animatedProperty, role, value));
    }

    static Ref<SVGMatrix> create(const SVGMatrixValue& initialValue = { })
    {
        return adoptRef(*new SVGMatrix(initialValue));
    }

    static Ref<SVGMatrix> create(const SVGMatrixValue* initialValue)
    {
        return adoptRef(*new SVGMatrix(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGMatrix>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    double a()
    {
        return propertyReference().a();
    }

    ExceptionOr<void> setA(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setA(value);
        commitChange();

        return { };
    }

    double b()
    {
        return propertyReference().b();
    }

    ExceptionOr<void> setB(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setB(value);
        commitChange();

        return { };
    }

    double c()
    {
        return propertyReference().c();
    }

    ExceptionOr<void> setC(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setC(value);
        commitChange();

        return { };
    }

    double d()
    {
        return propertyReference().d();
    }

    ExceptionOr<void> setD(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setD(value);
        commitChange();

        return { };
    }

    double e()
    {
        return propertyReference().e();
    }

    ExceptionOr<void> setE(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setE(value);
        commitChange();

        return { };
    }

    double f()
    {
        return propertyReference().f();
    }

    ExceptionOr<void> setF(double value)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setF(value);
        commitChange();

        return { };
    }

    ExceptionOr<Ref<SVGMatrix>> multiply(SVGMatrix& secondMatrix)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().multiply(secondMatrix.propertyReference());
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> inverse()
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().inverse();
        if (result.hasException())
            return result.releaseException();
        
        commitChange();
        return SVGMatrix::create(result.releaseReturnValue());
    }

    ExceptionOr<Ref<SVGMatrix>> translate(float x, float y)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().translate(x, y);        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> scale(float scaleFactor)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().scale(scaleFactor);        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> scaleNonUniform(float scaleFactorX, float scaleFactorY)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().scaleNonUniform(scaleFactorX, scaleFactorY);        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> rotate(float angle)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().rotate(angle);        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> rotateFromVector(float x, float y)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().rotateFromVector(x, y);        
        if (result.hasException())
            return result.releaseException();
        
        commitChange();
        return SVGMatrix::create(result.releaseReturnValue());
    }

    ExceptionOr<Ref<SVGMatrix>> flipX()
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().flipX();        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> flipY()
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().flipY();        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> skewX(float angle)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().skewX(angle);        
        commitChange();

        return SVGMatrix::create(result);
    }

    ExceptionOr<Ref<SVGMatrix>> skewY(float angle)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto result = propertyReference().skewY(angle);        
        commitChange();

        return SVGMatrix::create(result);
    }

protected:
    SVGMatrix(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGMatrixValue& value)
        : SVGPropertyTearOff<SVGMatrixValue>(&animatedProperty, role, value)
    {
    }

    explicit SVGMatrix(const SVGMatrixValue& initialValue)
        : SVGPropertyTearOff<SVGMatrixValue>(initialValue)
    {
    }

    explicit SVGMatrix(const SVGMatrixValue* initialValue)
        : SVGPropertyTearOff<SVGMatrixValue>(initialValue)
    {
    }
};

} // namespace WebCore
