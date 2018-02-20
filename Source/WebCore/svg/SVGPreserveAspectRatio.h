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

#include "SVGPreserveAspectRatioValue.h"
#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGPreserveAspectRatio : public SVGPropertyTearOff<SVGPreserveAspectRatioValue> {
public:
    static Ref<SVGPreserveAspectRatio> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGPreserveAspectRatioValue& value)
    {
        return adoptRef(*new SVGPreserveAspectRatio(animatedProperty, role, value));
    }

    static Ref<SVGPreserveAspectRatio> create(const SVGPreserveAspectRatioValue& initialValue = { })
    {
        return adoptRef(*new SVGPreserveAspectRatio(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGPreserveAspectRatio>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    unsigned short align()
    {
        return propertyReference().align();
    }

    ExceptionOr<void> setAlign(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = propertyReference().setAlign(value);
        if (result.hasException())
            return result;

        commitChange();
        return result;
    }

    unsigned short meetOrSlice()
    {
        return propertyReference().meetOrSlice();
    }

    ExceptionOr<void> setMeetOrSlice(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        auto result = propertyReference().setMeetOrSlice(value);
        if (result.hasException())
            return result;

        commitChange();
        return result;
    }

private:
    SVGPreserveAspectRatio(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, SVGPreserveAspectRatioValue& value)
        : SVGPropertyTearOff<SVGPreserveAspectRatioValue>(&animatedProperty, role, value)
    {
    }

    explicit SVGPreserveAspectRatio(const SVGPreserveAspectRatioValue& initialValue)
        : SVGPropertyTearOff<SVGPreserveAspectRatioValue>(initialValue)
    {
    }
};

} // namespace WebCore
