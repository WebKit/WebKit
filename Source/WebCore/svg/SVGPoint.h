/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
#include "FloatPoint.h"
#include "SVGMatrix.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGPoint : public SVGPropertyTearOff<FloatPoint> {
public:
    static Ref<SVGPoint> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, FloatPoint& value)
    {
        return adoptRef(*new SVGPoint(animatedProperty, role, value));
    }

    static Ref<SVGPoint> create(const FloatPoint& initialValue = { })
    {
        return adoptRef(*new SVGPoint(initialValue));
    }

    static Ref<SVGPoint> create(const FloatPoint* initialValue)
    {
        return adoptRef(*new SVGPoint(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGPoint>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    float x()
    {
        return propertyReference().x();
    }

    ExceptionOr<void> setX(float xValue)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setX(xValue);
        commitChange();

        return { };
    }

    float y()
    {
        return propertyReference().y();
    }

    ExceptionOr<void> setY(float xValue)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        propertyReference().setY(xValue);
        commitChange();

        return { };
    }

    ExceptionOr<Ref<SVGPoint>> matrixTransform(SVGMatrix& matrix)
    {
        if (isReadOnly())
            return Exception { NO_MODIFICATION_ALLOWED_ERR };

        auto newPoint = propertyReference().matrixTransform(matrix.propertyReference());
        commitChange();

        return SVGPoint::create(newPoint);
    }

protected:
    SVGPoint(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, FloatPoint& value)
        : SVGPropertyTearOff<FloatPoint>(&animatedProperty, role, value)
    {
    }

    SVGPoint(SVGPropertyRole role, FloatPoint& value)
        : SVGPropertyTearOff<FloatPoint>(nullptr, role, value)
    {
    }

    explicit SVGPoint(const FloatPoint& initialValue)
        : SVGPropertyTearOff<FloatPoint>(initialValue)
    {
    }

    explicit SVGPoint(const FloatPoint* initialValue)
        : SVGPropertyTearOff<FloatPoint>(initialValue)
    {
    }
};

} // namespace WebCore
