/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013, 2014 Igalia S.L.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "StyleGridTrackBreadth.h"
#include "StyleValueTypes.h"

namespace WebCore {
namespace Style {

using namespace CSS::Literals;

struct GridTrackMinMaxParameters {
    GridTrackBreadth min;
    GridTrackBreadth max;

    bool operator==(const GridTrackMinMaxParameters&) const = default;
};
using GridTrackMinMax = FunctionNotation<CSSValueMinmax, GridTrackMinMaxParameters>;

template<size_t I> const auto& get(const GridTrackMinMaxParameters& value)
{
    if constexpr (!I)
        return value.min;
    else if constexpr (I == 1)
        return value.max;
}

struct GridTrackFitContentLength : LengthWrapperBase<LengthPercentage<CSS::Nonnegative>> {
    using Base::Base;
};
using GridTrackFitContent = FunctionNotation<CSSValueFitContent, GridTrackFitContentLength>;

// This class represents a <track-size> from the spec. Although there are 3 different types of
// <track-size> there is always an equivalent minmax() representation that could represent any of
// them. The only special case is fit-content(argument) which is similar to minmax(auto,
// max-content) except that the track size is clamped at argument if it is greater than the auto
// minimum. At the GridTrackSize level we don't need to worry about clamping so we treat that case
// exactly as auto.
//
// We're using a separate attribute to store fit-content argument even though we could directly use
// m_maxTrackBreadth. The reason why we don't do it is because the maxTrackBreadth() call is a hot
// spot, so adding a conditional statement there (to distinguish between fit-content and any other
// case) was causing a severe performance drop.

// <track-size> = <track-breadth> | minmax( <inflexible-breadth> , <track-breadth> ) | fit-content( <length-percentage [0,∞]> )
// https://www.w3.org/TR/css-grid-2/#typedef-track-size
struct GridTrackSize {
    using Breadth = GridTrackBreadth;
    using MinMax = GridTrackMinMax;
    using FitContent = GridTrackFitContent;

    GridTrackSize(CSS::Keyword::Auto keyword)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(keyword)
        , m_maxTrackBreadth(keyword)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(CSS::ValueLiteral<CSS::LengthUnit::Px> literal)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(literal)
        , m_maxTrackBreadth(literal)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(CSS::ValueLiteral<CSS::PercentageUnit::Percentage> literal)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(literal)
        , m_maxTrackBreadth(literal)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(CSS::ValueLiteral<CSS::FlexUnit::Fr> literal)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(literal)
        , m_maxTrackBreadth(literal)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(const Breadth& breadth)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(breadth)
        , m_maxTrackBreadth(breadth)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(Breadth&& breadth)
        : m_type(Type::Breadth)
        , m_minTrackBreadth(breadth)
        , m_maxTrackBreadth(WTFMove(breadth))
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(const MinMax& minMax)
        : m_type(Type::MinMax)
        , m_minTrackBreadth(minMax->min)
        , m_maxTrackBreadth(minMax->max)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(MinMax&& minMax)
        : m_type(Type::MinMax)
        , m_minTrackBreadth(WTFMove(minMax->min))
        , m_maxTrackBreadth(WTFMove(minMax->max))
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(const FitContent& fitContent)
        : m_type(Type::FitContent)
        , m_fitContentTrackLength(*fitContent)
    {
        cacheMinMaxTrackBreadthTypes();
    }

    GridTrackSize(FitContent&& fitContent)
        : m_type(Type::FitContent)
        , m_fitContentTrackLength(WTFMove(*fitContent))
    {
        cacheMinMaxTrackBreadthTypes();
    }

    const GridTrackFitContentLength& fitContentTrackLength() const { ASSERT(m_type == Type::FitContent); return m_fitContentTrackLength; }

    const GridTrackBreadth& minTrackBreadth() const { return m_minTrackBreadth; }
    const GridTrackBreadth& maxTrackBreadth() const { return m_maxTrackBreadth; }

    bool isBreadth() const { return m_type == Type::Breadth; }
    bool isMinMax() const { return m_type == Type::MinMax; }
    bool isFitContent() const { return m_type == Type::FitContent; }

    bool isContentSized() const { return m_minTrackBreadth.isContentSized() || m_maxTrackBreadth.isContentSized(); }

    template<typename... F> decltype(auto) switchOn(F&&...) const;

    bool operator==(const GridTrackSize& other) const
    {
        return m_type == other.m_type
            && m_minTrackBreadth == other.m_minTrackBreadth
            && m_maxTrackBreadth == other.m_maxTrackBreadth
            && m_fitContentTrackLength == other.m_fitContentTrackLength;
    }

    void cacheMinMaxTrackBreadthTypes()
    {
        m_minTrackBreadthIsAuto = minTrackBreadth().isLength() && minTrackBreadth().length().isAuto();
        m_minTrackBreadthIsMinContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMinContent();
        m_minTrackBreadthIsMaxContent = minTrackBreadth().isLength() && minTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMaxContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMaxContent();
        m_maxTrackBreadthIsMinContent = maxTrackBreadth().isLength() && maxTrackBreadth().length().isMinContent();
        m_maxTrackBreadthIsAuto = maxTrackBreadth().isLength() && maxTrackBreadth().length().isAuto();
        m_maxTrackBreadthIsFixed = maxTrackBreadth().isLength() && maxTrackBreadth().length().isSpecified();

        // These values depend on the above ones so keep them here.
        m_minTrackBreadthIsIntrinsic = m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent
            || m_minTrackBreadthIsAuto || isFitContent();
        m_maxTrackBreadthIsIntrinsic = m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsMinContent
            || m_maxTrackBreadthIsAuto || isFitContent();
    }

    bool hasIntrinsicMinTrackBreadth() const { return m_minTrackBreadthIsIntrinsic; }
    bool hasIntrinsicMaxTrackBreadth() const { return m_maxTrackBreadthIsIntrinsic; }
    bool hasMinOrMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent || m_minTrackBreadthIsMinContent; }
    bool hasAutoMinTrackBreadth() const { return m_minTrackBreadthIsAuto; }
    bool hasAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsAuto; }
    bool hasMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent; }
    bool hasMaxContentOrAutoMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsAuto; }
    bool hasMinContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMinContent; }
    bool hasMinOrMaxContentMaxTrackBreadth() const { return m_maxTrackBreadthIsMaxContent || m_maxTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadth() const { return m_minTrackBreadthIsMaxContent; }
    bool hasMinContentMinTrackBreadth() const { return m_minTrackBreadthIsMinContent; }
    bool hasMaxContentMinTrackBreadthAndMaxContentMaxTrackBreadth() const { return m_minTrackBreadthIsMaxContent && m_maxTrackBreadthIsMaxContent; }
    bool hasAutoOrMinContentMinTrackBreadthAndIntrinsicMaxTrackBreadth() const { return (m_minTrackBreadthIsMinContent || m_minTrackBreadthIsAuto) && m_maxTrackBreadthIsIntrinsic; }
    bool hasFixedMaxTrackBreadth() const { return m_maxTrackBreadthIsFixed; }

private:
    friend struct Blending<GridTrackSize>;

    enum class Type : uint8_t {
        Breadth,
        MinMax,
        FitContent
    };
    Type type() const { return m_type; }

    Type m_type { Type::Breadth };
    GridTrackBreadth m_minTrackBreadth { CSS::Keyword::Auto { } };
    GridTrackBreadth m_maxTrackBreadth { CSS::Keyword::Auto { } };
    GridTrackFitContentLength m_fitContentTrackLength { 0_css_px };

    bool m_minTrackBreadthIsAuto : 1;
    bool m_maxTrackBreadthIsAuto : 1;
    bool m_minTrackBreadthIsMaxContent : 1;
    bool m_minTrackBreadthIsMinContent : 1;
    bool m_maxTrackBreadthIsMaxContent : 1;
    bool m_maxTrackBreadthIsMinContent : 1;
    bool m_minTrackBreadthIsIntrinsic : 1;
    bool m_maxTrackBreadthIsIntrinsic : 1;
    bool m_maxTrackBreadthIsFixed : 1;
};

template<typename... F> decltype(auto) GridTrackSize::switchOn(F&&... f) const
{
    auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

    switch (m_type) {
    case Type::Breadth:
        return visitor(m_minTrackBreadth);

    case Type::FitContent:
        return visitor(
            GridTrackSize::FitContent {
                .parameters = m_fitContentTrackLength
            }
        );

    case Type::MinMax:
        if (m_minTrackBreadth.isAuto() && m_maxTrackBreadth.isFlex())
            return visitor(m_maxTrackBreadth.flex());

        return visitor(
            GridTrackSize::MinMax {
                .parameters = {
                    .min = m_minTrackBreadth,
                    .max = m_maxTrackBreadth,
                }
            }
        );
    }

    RELEASE_ASSERT_NOT_REACHED();
}

// MARK: - Conversion

template<> struct CSSValueConversion<GridTrackSize> { auto operator()(BuilderState&, const CSSValue&) -> GridTrackSize; };

// MARK: - Blending

template<> struct Blending<GridTrackSize> {
    auto blend(const GridTrackSize&, const GridTrackSize&, const BlendingContext&) -> GridTrackSize;
};

// MARK: - Logging

WTF::TextStream& operator<<(WTF::TextStream&, const GridTrackSize&);

} // namespace Style
} // namespace WebCore

DEFINE_COMMA_SEPARATED_TUPLE_LIKE_CONFORMANCE(WebCore::Style::GridTrackMinMaxParameters, 2)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridTrackFitContentLength)
DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::GridTrackSize)
