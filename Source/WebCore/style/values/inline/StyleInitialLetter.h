/*
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

#include <WebCore/StylePrimitiveNumericTypes.h>
#include <WebCore/StyleValueTypes.h>
#include <cmath>

namespace WebCore {
namespace Style {

// <'initial-letter'> = normal | <number [1,∞]> <integer [1,∞]> | <number [1,∞]> && [ drop | raise ]?
// https://drafts.csswg.org/css-inline/#propdef-initial-letter
struct InitialLetter {
    using Size = Number<CSS::Positive, float>;
    using Sink = Integer<CSS::Positive>;

    // How the sink (block-axis position) was specified. This is retained so the computed
    // value can be serialized back in its specified form.
    enum class SinkType : uint8_t { Normal, Omitted, Integer, Drop, Raise };

    constexpr InitialLetter(CSS::Keyword::Normal)
    {
    }

    constexpr InitialLetter(Size size)
        : m_size { size }
        , m_sinkType { SinkType::Omitted }
    {
    }

    constexpr InitialLetter(Size size, Sink sink)
        : m_size { size }
        , m_sink { sink }
        , m_sinkType { SinkType::Integer }
    {
    }

    constexpr InitialLetter(Size size, CSS::Keyword::Drop)
        : m_size { size }
        , m_sinkType { SinkType::Drop }
    {
    }

    constexpr InitialLetter(Size size, CSS::Keyword::Raise)
        : m_size { size }
        , m_sinkType { SinkType::Raise }
    {
    }

    constexpr bool isNormal() const { return m_sinkType == SinkType::Normal; }

    // The number of lines the initial letter spans.
    constexpr float height() const { return m_size ? m_size->value : 0; }

    // The effective sink: how far the initial letter descends, in lines. When the sink is
    // omitted or `drop`, it is the integer floor of the size; `raise` sinks a single line.
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
        case SinkType::Integer:
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
        case SinkType::Omitted:
            return visitor(*m_size);
        case SinkType::Integer:
            return visitor(SpaceSeparatedTuple { *m_size, *m_sink });
        case SinkType::Drop:
            return visitor(SpaceSeparatedTuple { *m_size, CSS::Keyword::Drop { } });
        case SinkType::Raise:
            return visitor(SpaceSeparatedTuple { *m_size, CSS::Keyword::Raise { } });
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    bool operator==(const InitialLetter&) const = default;

private:
    Markable<Size> m_size;
    Markable<Sink> m_sink;
    SinkType m_sinkType { SinkType::Normal };
};

// MARK: - Conversion

template<> struct CSSValueConversion<InitialLetter> {
    auto operator()(BuilderState&, const CSSValue&) -> InitialLetter;
};

} // namespace Style
} // namespace WebCore

DEFINE_VARIANT_LIKE_CONFORMANCE(WebCore::Style::InitialLetter)
