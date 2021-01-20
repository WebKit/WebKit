/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include "SVGMatrix.h"
#include "SVGPropertyTraits.h"
#include "SVGValueProperty.h"

namespace WebCore {

class SVGPoint : public SVGValueProperty<FloatPoint> {
    using Base = SVGValueProperty<FloatPoint>;
    using Base::Base;
    using Base::m_value;

public:
    static Ref<SVGPoint> create(const FloatPoint& value = { })
    {
        return adoptRef(*new SVGPoint(value));
    }

    template<typename T>
    static ExceptionOr<Ref<SVGPoint>> create(ExceptionOr<T>&& value)
    {
        if (value.hasException())
            return value.releaseException();
        return adoptRef(*new SVGPoint(value.releaseReturnValue()));
    }

    Ref<SVGPoint> clone() const
    {
        return SVGPoint::create(m_value);
    }
    
    float x() { return m_value.x(); }

    ExceptionOr<void> setX(float x)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setX(x);
        commitChange();

        return { };
    }

    float y() { return m_value.y(); }

    ExceptionOr<void> setY(float y)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value.setY(y);
        commitChange();
        return { };
    }

    Ref<SVGPoint> matrixTransform(SVGMatrix& matrix) const
    {
        auto newPoint = m_value.matrixTransform(matrix.value());
        return adoptRef(*new SVGPoint(newPoint));
    }

private:
    String valueAsString() const override
    {
        return SVGPropertyTraits<FloatPoint>::toString(m_value);
    }
};

} // namespace WebCore
