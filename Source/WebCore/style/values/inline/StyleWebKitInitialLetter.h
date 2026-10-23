/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 ChangSeok Oh <changseok@webkit.org>
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

#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>
#include <cmath>

namespace WebCore {
namespace Style {

// Shared storage for both initial-letter properties:
//
//   <'initial-letter'>         = normal | <number [1,∞]> <integer [1,∞]>
//                              | <number [1,∞]> && [ drop | raise ]?
//   <'-webkit-initial-letter'> = normal | <number [0,∞]>{1,2}@(default=previous)
//
// The grammars disagree: the legacy property takes a fractional sink (deliberately, see
// 298564@main), a bare number duplicates the size rather than meaning `drop`, and it sinks
// one line less in the raised/sunken case. SinkType records which spelling produced the
// value so serialization and layout can tell them apart.
//
// https://drafts.csswg.org/css-inline/#propdef-initial-letter
struct WebkitInitialLetter {
    // The union of both grammars' domains, hence not Positive/Integer.
    using Size = Number<CSS::Nonnegative, float>;
    using Sink = Number<CSS::Nonnegative, float>;

    enum class SinkType : uint8_t { Normal, Legacy, Omitted, Numeric, Drop, Raise };

    // Disambiguates the legacy constructor from the Numeric one.
    struct LegacyTag { };

    constexpr WebkitInitialLetter(CSS::Keyword::Normal)
    {
    }

    constexpr WebkitInitialLetter(LegacyTag, Size size, Sink sink)
        : m_size { size }
        , m_sink { sink }
        , m_sinkType { SinkType::Legacy }
    {
    }

    constexpr WebkitInitialLetter(Size size)
        : m_size { size }
        , m_sinkType { SinkType::Omitted }
    {
    }

    constexpr WebkitInitialLetter(Size size, Sink sink)
        : m_size { size }
        , m_sink { sink }
        , m_sinkType { SinkType::Numeric }
    {
    }

    constexpr WebkitInitialLetter(Size size, CSS::Keyword::Drop)
        : m_size { size }
        , m_sinkType { SinkType::Drop }
    {
    }

    constexpr WebkitInitialLetter(Size size, CSS::Keyword::Raise)
        : m_size { size }
        , m_sinkType { SinkType::Raise }
    {
    }

    constexpr bool isNormal() const { return m_sinkType == SinkType::Normal; }

    constexpr bool isLegacy() const { return m_sinkType == SinkType::Legacy; }

    constexpr float height() const { return m_size ? m_size->value : 0; }

    // How far the letter descends, in lines.
    float drop() const
    {
        switch (m_sinkType) {
        case SinkType::Normal:
            return 0;
        case SinkType::Omitted:
        case SinkType::Drop:
            return std::floor(m_size->value);
        case SinkType::Raise:
            return 1;
        case SinkType::Legacy:
        case SinkType::Numeric:
            return m_sink->value;
        }
        ASSERT_NOT_REACHED();
        return 0;
    }

    template<typename... F> decltype(auto) switchOn(F&&... f) const
    {
        auto visitor = WTF::makeVisitor(std::forward<F>(f)...);

        switch (m_sinkType) {
        case SinkType::Normal:
            return visitor(CSS::Keyword::Normal { });
        case SinkType::Legacy:
            // Collapses to one number when size and sink agree, as the legacy property did.
            return visitor(MinimallySerializingSpaceSeparatedPair<Sink> { *m_size, *m_sink });
        case SinkType::Omitted:
            return visitor(*m_size);
        case SinkType::Numeric:
            return visitor(SpaceSeparatedTuple { *m_size, *m_sink });
        case SinkType::Drop:
            return visitor(SpaceSeparatedTuple { *m_size, CSS::Keyword::Drop { } });
        case SinkType::Raise:
            return visitor(SpaceSeparatedTuple { *m_size, CSS::Keyword::Raise { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const WebkitInitialLetter&) const = default;

private:
    Markable<Size> m_size;
    Markable<Sink> m_sink;
    SinkType m_sinkType { SinkType::Normal };
};

// MARK: - Conversion

template<> struct CSSValueConversion<WebkitInitialLetter> {
    auto operator()(BuilderState&, const CSSValue&) -> WebkitInitialLetter;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::WebkitInitialLetter)
