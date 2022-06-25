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

#include "ExceptionOr.h"
#include "SVGPropertyTraits.h"
#include "SVGValueProperty.h"

namespace WebCore {

class SVGNumber : public SVGValueProperty<float> {
    using Base = SVGValueProperty<float>;
    using Base::Base;
    using Base::m_value;
        
public:
    static Ref<SVGNumber> create(float value = 0)
    {
        return adoptRef(*new SVGNumber(value));
    }

    static Ref<SVGNumber> create(SVGPropertyOwner* owner, SVGPropertyAccess access, float value = 0)
    {
        return adoptRef(*new SVGNumber(owner, access, value));
    }

    template<typename T>
    static ExceptionOr<Ref<SVGNumber>> create(ExceptionOr<T>&& value)
    {
        if (value.hasException())
            return value.releaseException();
        return adoptRef(*new SVGNumber(value.releaseReturnValue()));
    }

    Ref<SVGNumber> clone() const
    {
        return SVGNumber::create(m_value);
    }

    float valueForBindings()
    {
        return m_value;
    }

    ExceptionOr<void> setValueForBindings(float value)
    {
        if (isReadOnly())
            return Exception { NoModificationAllowedError };

        m_value = value;
        commitChange();
        return { };
    }

    String valueAsString() const override
    {
        return SVGPropertyTraits<float>::toString(m_value);
    }
};

} // namespace WebCore
