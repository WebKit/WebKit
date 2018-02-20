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

#include "SVGPropertyTearOff.h"

namespace WebCore {

class SVGNumber : public SVGPropertyTearOff<float> {
public:
    static Ref<SVGNumber> create(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, float& value)
    {
        return adoptRef(*new SVGNumber(animatedProperty, role, value));
    }

    static Ref<SVGNumber> create(const float& initialValue = { })
    {
        return adoptRef(*new SVGNumber(initialValue));
    }

    template<typename T> static ExceptionOr<Ref<SVGNumber>> create(ExceptionOr<T>&& initialValue)
    {
        if (initialValue.hasException())
            return initialValue.releaseException();
        return create(initialValue.releaseReturnValue());
    }

    float valueForBindings()
    {
        return propertyReference();
    }

    ExceptionOr<void> setValueForBindings(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        propertyReference() = value;
        commitChange();

        return { };
    }

private:
    SVGNumber(SVGAnimatedProperty& animatedProperty, SVGPropertyRole role, float& value)
        : SVGPropertyTearOff<float>(&animatedProperty, role, value)
    {
    }

    explicit SVGNumber(const float& initialValue)
        : SVGPropertyTearOff<float>(initialValue)
    {
    }
};

} // namespace WebCore
