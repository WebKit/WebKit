/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RotateTransformOperation.h"
#include "StylePrimitiveNumericTypes.h"
#include <wtf/PointerComparison.h>

namespace WebCore {
namespace Style {

// <'rotate'> = none | <angle> | [ x | y | z | <number>{3} ] && <angle>
// https://drafts.csswg.org/css-transforms-2/#propdef-rotate
struct Rotate {
    Rotate(CSS::Keyword::None)
        : m_value { }
    {
    }

    Rotate(Ref<RotateTransformOperation> value)
        : m_value { WTFMove(value) }
    {
    }

    bool isNone() const { return !m_value; }

    RotateTransformOperation* operation() const { return m_value.get(); }
    RotateTransformOperation* operator->() const { return operation(); }

    template<typename F> decltype(auto) switchOn(F&& functor) const
    {
        RefPtr value = m_value;
        if (!value)
            return functor(CSS::Keyword::None { });

        if (!value->is3DOperation() || (!value->x() && !value->y() && value->z()))
            return functor(Angle<> { value->angle() });
        if (value->x() && !value->y() && !value->z())
            return functor(SpaceSeparatedTuple { CSS::Keyword::X { }, Angle<> { value->angle() } });
        if (!value->x() && value->y() && !value->z())
            return functor(SpaceSeparatedTuple { CSS::Keyword::Y { }, Angle<> { value->angle() } });
        return functor(
            SpaceSeparatedTuple {
                Number<> { value->x() },
                Number<> { value->y() },
                Number<> { value->z() },
                Angle<> { value->angle() },
            }
        );
    }

    bool operator==(const Rotate& other) const
    {
        return arePointingToEqualData(m_value, other.m_value);
    }

private:
    RefPtr<RotateTransformOperation> m_value;
};

template<> struct Blending<Rotate> {
    constexpr auto canBlend(const Rotate&, const Rotate&) -> bool { return true; }
    auto blend(const Rotate&, const Rotate&, const BlendingContext&) -> Rotate;
};

} // namespace Style
} // namespace WebCore

template<> inline constexpr auto WebCore::TreatAsVariantLike<WebCore::Style::Rotate> = true;
