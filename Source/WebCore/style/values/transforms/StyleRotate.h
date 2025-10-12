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

#include <WebCore/StyleRotateTransformFunction.h>
#include <WebCore/StyleTransformFunctionWrapper.h>

namespace WebCore {
namespace Style {

// <'rotate'> = none | <angle> | [ x | y | z | <number>{3} ] && <angle>
// https://drafts.csswg.org/css-transforms-2/#propdef-rotate
struct Rotate {
    struct Function : TransformFunctionWrapper<RotateTransformFunction> {
        using TransformFunctionWrapper<RotateTransformFunction>::TransformFunctionWrapper;

        template<typename... F> decltype(auto) switchOn(F&&...) const;
    };

    Rotate(CSS::Keyword::None) : value { nullptr } { }
    Rotate(Function&& value) : value { WTFMove(value.value) } { }
    Rotate(Ref<const RotateTransformFunction>&& value) : value { WTFMove(value) } { }

    bool affectedByTransformOrigin() const { return value && !value->isIdentity(); }
    bool isRepresentableIn2D() const { return !value || value->isRepresentableIn2D(); }
    bool is3DOperation() const { return value && value->is3DOperation(); }

    void apply(TransformationMatrix&, const FloatSize&) const;

    bool isNone() const { return !value; }
    bool isFunction() const { return !!value; }

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const Rotate& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

private:
    friend struct Blending<Rotate>;
    friend struct ToPlatform<Rotate>;

    RefPtr<const RotateTransformFunction> value;
};

// MARK: Rotate Function

template<typename... F> decltype(auto) Rotate::Function::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    Ref protectedValue = value;
    if (!protectedValue->is3DOperation() || (protectedValue->x().isZero() && protectedValue->y().isZero() && !protectedValue->z().isZero()))
        return visitor(protectedValue->angle());
    if (!protectedValue->x().isZero() && protectedValue->y().isZero() && protectedValue->z().isZero())
        return visitor(SpaceSeparatedTuple { CSS::Keyword::X { }, protectedValue->angle() });
    if (protectedValue->x().isZero() && !protectedValue->y().isZero() && protectedValue->z().isZero())
        return visitor(SpaceSeparatedTuple { CSS::Keyword::Y { }, protectedValue->angle() });
    return visitor(
        SpaceSeparatedTuple {
            protectedValue->x(),
            protectedValue->y(),
            protectedValue->z(),
            protectedValue->angle(),
        }
    );
}

// MARK: Rotate

template<typename T> bool Rotate::holdsAlternative() const
{
         if constexpr (std::same_as<T, CSS::Keyword::None>) return isNone();
    else if constexpr (std::same_as<T, Function>)           return isFunction();
}

template<typename... F> decltype(auto) Rotate::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    if (!value)
        return visitor(CSS::Keyword::None { });
    return visitor(Function { *value });
}

// MARK: - Conversion

template<> struct CSSValueConversion<Rotate> { auto operator()(BuilderState&, const CSSValue&) -> Rotate; };

// MARK: - Blending

template<> struct Blending<Rotate> {
    auto blend(const Rotate&, const Rotate&, const BlendingContext&) -> Rotate;
};

// MARK: - Platform

template<> struct ToPlatform<Rotate> { auto operator()(const Rotate&, const FloatSize&) -> RefPtr<TransformOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Rotate::Function)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Rotate)
