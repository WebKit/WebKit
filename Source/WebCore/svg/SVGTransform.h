/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "SVGMatrix.h"
#include "SVGPropertyTearOff.h"
#include "SVGTransformValue.h"

namespace WebCore {

class SVGTransform : public SVGPropertyTearOff<SVGTransformValue> {
public:
    static Ref<SVGTransform> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGTransformValue& value)
    {
        return adoptRef(*new SVGTransform(animatedProperty, role, value));
    }

    static Ref<SVGTransform> create(const SVGTransformValue& initialValue = { })
    {
        return adoptRef(*new SVGTransform(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGTransform>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    unsigned short type()
    {
        return propertyReference().type();
    }

    Ref<SVGMatrix> matrix();

    float angle()
    {
        return propertyReference().angle();
    }

    ExceptionOr<void> setMatrix(SVGMatrix& matrix)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setMatrix(matrix.propertyReference());
        commitChange();

        return { };
    }

    ExceptionOr<void> setTranslate(float tx, float ty)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setTranslate(tx, ty);
        commitChange();

        return { };
    }

    ExceptionOr<void> setScale(float sx, float sy)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setScale(sx, sy);
        commitChange();

        return { };
    }

    ExceptionOr<void> setRotate(float angle, float cx, float cy)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setRotate(angle, cx, cy);
        commitChange();

        return { };
    }

    ExceptionOr<void> setSkewX(float angle)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setSkewX(angle);
        commitChange();

        return { };
    }

    ExceptionOr<void> setSkewY(float angle)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference().setSkewY(angle);
        commitChange();

        return { };
    }

private:
    SVGTransform(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGTransformValue& value)
        : SVGPropertyTearOff<SVGTransformValue>(&animatedProperty, role, value)
    {
    }

    explicit SVGTransform(const SVGTransformValue& initialValue)
        : SVGPropertyTearOff<SVGTransformValue>(initialValue)
    {
    }
};

} // namespace WebCore
