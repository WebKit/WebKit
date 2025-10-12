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

#include <WebCore/StyleTransformFunctionWrapper.h>
#include <WebCore/StyleTranslateTransformFunction.h>

namespace WebCore {
namespace Style {

// <'translate'> = none | <length-percentage> [ <length-percentage> <length>? ]?
// https://drafts.csswg.org/css-transforms-2/#propdef-translate
struct Translate {
    struct Function : TransformFunctionWrapper<TranslateTransformFunction> {
        using TransformFunctionWrapper<TranslateTransformFunction>::TransformFunctionWrapper;

        template<typename... F> decltype(auto) switchOn(F&&...) const;
    };

    Translate(CSS::Keyword::None) : value { nullptr } { }
    Translate(Function&& value) : value { WTFMove(value.value) } { }
    Translate(Ref<const TranslateTransformFunction>&& value) : value { WTFMove(value) } { }

    bool isRepresentableIn2D() const { return !value || value->isRepresentableIn2D(); }
    bool is3DOperation() const { return value && value->is3DOperation(); }

    TransformFunctionSizeDependencies computeSizeDependencies() const;

    void apply(TransformationMatrix&, const FloatSize&) const;

    bool isNone() const { return !value; }
    bool isFunction() const { return !!value; }

    template<typename> bool holdsAlternative() const;
    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const Translate& other) const
    {
        return arePointingToEqualData(value, other.value);
    }

private:
    friend struct Blending<Translate>;
    friend struct ToPlatform<Translate>;

    RefPtr<const TranslateTransformFunction> value;
};

// MARK: Translate Operation

template<typename... F> decltype(auto) Translate::Function::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    Ref protectedValue = value;
    if (!protectedValue->z().isZero())
        return visitor(SpaceSeparatedTuple { protectedValue->x(), protectedValue->y(), protectedValue->z() });
    if (!protectedValue->y().isKnownZero() || protectedValue->y().isPercent())
        return visitor(SpaceSeparatedTuple { protectedValue->x(), protectedValue->y() });
    return visitor(SpaceSeparatedTuple { protectedValue->x() });
}

// MARK: Translate

template<typename T> bool Translate::holdsAlternative() const
{
         if constexpr (std::same_as<T, CSS::Keyword::None>) return isNone();
    else if constexpr (std::same_as<T, Function>)           return isFunction();
}

template<typename... F> decltype(auto) Translate::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    if (!value)
        return visitor(CSS::Keyword::None { });
    return visitor(Function { *value });
}

// MARK: - Conversion

template<> struct CSSValueConversion<Translate> { auto operator()(BuilderState&, const CSSValue&) -> Translate; };

// MARK: - Blending

template<> struct Blending<Translate> {
    auto blend(const Translate&, const Translate&, const BlendingContext&) -> Translate;
};

// MARK: - Platform

template<> struct ToPlatform<Translate> { auto operator()(const Translate&, const FloatSize&) -> RefPtr<TransformOperation>; };

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Translate::Function)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::Translate)
