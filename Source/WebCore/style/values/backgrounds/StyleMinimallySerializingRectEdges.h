/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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

#include "CSSMinimallySerializingRectEdges.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

template<typename T> struct MinimallySerializingRectEdges {
    MinimallySerializingRectEdges(RectEdges<T> edges)
        : value { WTFMove(edges) }
    {
    }

    MinimallySerializingRectEdges(T top, T right, T bottom, T left)
        : value { WTFMove(top), WTFMove(right), WTFMove(bottom), WTFMove(left) }
    {
    }

    const T& top() const    { return value.top(); }
    const T& right() const  { return value.right(); }
    const T& bottom() const { return value.bottom(); }
    const T& left() const   { return value.left(); }

    constexpr bool operator==(const MinimallySerializingRectEdges<T>&) const = default;

    RectEdges<T> value;
};

template<size_t I, typename T> decltype(auto) get(const MinimallySerializingRectEdges<T>& value)
{
    if constexpr (!I)
        return value.top();
    else if constexpr (I == 1)
        return value.right();
    else if constexpr (I == 2)
        return value.bottom();
    else if constexpr (I == 3)
        return value.left();
}

template<typename StyleType> struct ToCSS<MinimallySerializingRectEdges<StyleType>> {
    using Result = CSS::MinimallySerializingRectEdges<CSSType<StyleType>>;

    decltype(auto) operator()(const MinimallySerializingRectEdges<StyleType>& value, const RenderStyle& style)
    {
        return Result {
            toCSS(value.top(), style),
            toCSS(value.right(), style),
            toCSS(value.bottom(), style),
            toCSS(value.left(), style)
        };
    }
};

template<typename CSSType> struct ToStyle<CSS::MinimallySerializingRectEdges<CSSType>> {
    using Result = MinimallySerializingRectEdges<StyleType<CSSType>>;

    decltype(auto) operator()(const CSS::MinimallySerializingRectEdges<CSSType>& value, const CSSToLengthConversionData& conversionData, const CSSCalcSymbolTable& symbolTable)
    {
        return Result {
            toStyle(value.top(), conversionData, symbolTable),
            toStyle(value.right(), conversionData, symbolTable),
            toStyle(value.bottom(), conversionData, symbolTable),
            toStyle(value.left(), conversionData, symbolTable)
        };
    }
    decltype(auto) operator()(const CSS::MinimallySerializingRectEdges<CSSType>& value, const BuilderState& builderState, const CSSCalcSymbolTable& symbolTable)
    {
        return Result {
            toStyle(value.top(), builderState, symbolTable),
            toStyle(value.right(), builderState, symbolTable),
            toStyle(value.bottom(), builderState, symbolTable),
            toStyle(value.left(), builderState, symbolTable)
        };
    }
    decltype(auto) operator()(const CSS::MinimallySerializingRectEdges<CSSType>& value, NoConversionDataRequiredToken, const CSSCalcSymbolTable& symbolTable)
    {
        return Result {
            toStyleNoConversionDataRequired(value.top(), symbolTable),
            toStyleNoConversionDataRequired(value.right(), symbolTable),
            toStyleNoConversionDataRequired(value.bottom(), symbolTable),
            toStyleNoConversionDataRequired(value.left(), symbolTable)
        };
    }
};

template<typename StyleType> struct Blending<Style::MinimallySerializingRectEdges<StyleType>> {
    auto canBlend(const Style::MinimallySerializingRectEdges<StyleType>& a, const Style::MinimallySerializingRectEdges<StyleType>& b) -> bool
    {
        return WebCore::Style::canBlend(a.top(), b.top())
            && WebCore::Style::canBlend(a.right(), b.right())
            && WebCore::Style::canBlend(a.bottom(), b.bottom())
            && WebCore::Style::canBlend(a.left(), b.left());
    }
    auto blend(const Style::MinimallySerializingRectEdges<StyleType>& a, const Style::MinimallySerializingRectEdges<StyleType>& b, const BlendingContext& context) -> Style::MinimallySerializingRectEdges<StyleType>
    {
        return Style::MinimallySerializingRectEdges<StyleType> {
            WebCore::Style::blend(a.top(), b.top(), context),
            WebCore::Style::blend(a.right(), b.right(), context),
            WebCore::Style::blend(a.bottom(), b.bottom(), context),
            WebCore::Style::blend(a.left(), b.left(), context)
        };
    }
};

} // namespace Style
} // namespace WebCore

namespace std {

template<typename T> class tuple_size<WebCore::Style::MinimallySerializingRectEdges<T>> : public std::integral_constant<size_t, 4> { };
template<size_t I, typename T> class tuple_element<I, WebCore::Style::MinimallySerializingRectEdges<T>> {
public:
    using type = T;
};

}
