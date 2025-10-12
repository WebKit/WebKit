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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FontSizeAdjust.h>
#include <WebCore/StylePrimitiveNumericTypes.h>

namespace WebCore {
namespace Style {

// <'font-size-adjust'> = none | [ [ ex-height | cap-height | ch-width | ic-width | ic-height ]? [ from-font | <number [0,inf]> ] ]
// FIXME: Current spec grammar is `none | <number [0,∞]>`
// https://drafts.csswg.org/css-fonts-4/#propdef-font-size-adjust
struct FontSizeAdjust {
    using Number = Style::Number<CSS::Nonnegative>;
    using Metric = WebCore::FontSizeAdjust::Metric;
    using ValueType = WebCore::FontSizeAdjust::ValueType;

    FontSizeAdjust(CSS::Keyword::None) : m_platform { Metric::ExHeight } { }
    FontSizeAdjust(Metric metric, CSS::Keyword::FromFont) : m_platform { metric, ValueType::FromFont, std::nullopt } { }
    FontSizeAdjust(Metric metric, Number metricValue) : m_platform { metric, ValueType::Number, metricValue.value } { }

    FontSizeAdjust(WebCore::FontSizeAdjust platform) : m_platform { platform } { }

    bool isNone() const { return m_platform.isNone(); }
    bool isFromFont() const { return m_platform.isFromFont(); }

    Metric metric() const { return m_platform.metric; }
    std::optional<float> metricValue() const { return m_platform.value; }
    std::optional<float> resolvedMetricValue(const RenderStyle&) const;

    WebCore::FontSizeAdjust platform() const { return m_platform; }

    bool operator==(const FontSizeAdjust&) const = default;

private:
    WebCore::FontSizeAdjust m_platform;
};

// MARK: - Conversion

template<> struct CSSValueConversion<FontSizeAdjust> { auto operator()(BuilderState&, const CSSValue&) -> FontSizeAdjust; };
// NOTE: `FontSizeAdjust` is special-cased to allow resolution of `from-font`.
template<> struct CSSValueCreation<FontSizeAdjust> { auto operator()(CSSValuePool&, const RenderStyle&, const FontSizeAdjust&) -> Ref<CSSValue>; };

// MARK: - Serialization

// NOTE: `FontSizeAdjust` is special-cased to allow resolution of `from-font`.
template<> struct Serialize<FontSizeAdjust> { void operator()(StringBuilder&, const CSS::SerializationContext&, const RenderStyle&, const FontSizeAdjust&); };

// MARK: - Blending

template<> struct Blending<FontSizeAdjust> {
    auto canBlend(const FontSizeAdjust&, const FontSizeAdjust&) -> bool;
    auto blend(const FontSizeAdjust&, const FontSizeAdjust&, const BlendingContext&) -> FontSizeAdjust;
};

// MARK: - Logging

TextStream& operator<<(TextStream&, const FontSizeAdjust&);

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::FontSizeAdjust)
