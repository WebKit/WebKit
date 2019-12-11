/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "SVGDecoratedPrimitive.h"

namespace WebCore {

template<typename DecorationType, typename EnumType>
class SVGDecoratedEnumeration : public SVGDecoratedPrimitive<DecorationType, EnumType> {
    using Base = SVGDecoratedPrimitive<DecorationType, EnumType>;
    using Base::Base;
    using Base::m_value;

public:
    static auto create(const EnumType& value)
    {
        static_assert(std::is_integral<DecorationType>::value, "DecorationType form enum should be integral.");
        return makeUnique<SVGDecoratedEnumeration>(value);
    }

private:
    bool setValue(const DecorationType& value) override
    {
        if (!value || value > SVGIDLEnumLimits<EnumType>::highestExposedEnumValue())
            return false;
        Base::setValueInternal(value);
        return true;
    }

    DecorationType value() const override
    {
        if (Base::value() > SVGIDLEnumLimits<EnumType>::highestExposedEnumValue())
            return m_outOfRangeEnumValue;
        return Base::value();
    }

    std::unique_ptr<SVGDecoratedProperty<DecorationType>> clone() override
    {
        return create(m_value);
    }

    static const DecorationType m_outOfRangeEnumValue = 0;
};

}
