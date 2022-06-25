/*
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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

#include "Color.h"
#include "Gradient.h"
#include "Pattern.h"

namespace WebCore {

class SourceBrush {
public:
    struct Brush {
        struct LogicalGradient {
            Ref<Gradient> gradient;
            AffineTransform spaceTransform;

            template<typename Encoder> void encode(Encoder&) const;
            template<typename Decoder> static std::optional<LogicalGradient> decode(Decoder&);
        };

        std::variant<LogicalGradient, Ref<Pattern>> brush;

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<Brush> decode(Decoder&);
    };

    SourceBrush() = default;
    WEBCORE_EXPORT SourceBrush(const Color&, std::optional<Brush>&& = std::nullopt);

    const Color& color() const { return m_color; }
    void setColor(const Color color) { m_color = color; }

    const std::optional<Brush>& brush() const { return m_brush; }

    WEBCORE_EXPORT Gradient* gradient() const;
    WEBCORE_EXPORT Pattern* pattern() const;
    const AffineTransform& gradientSpaceTransform() const;

    void setGradient(Ref<Gradient>&&, const AffineTransform& spaceTransform = { });
    void setPattern(Ref<Pattern>&&);

    bool isInlineColor() const { return !m_brush && m_color.tryGetAsSRGBABytes(); }
    bool isVisible() const { return m_brush || m_color.isVisible(); }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<SourceBrush> decode(Decoder&);

private:
    Color m_color { Color::black };
    std::optional<Brush> m_brush;
};

inline bool operator==(const SourceBrush::Brush::LogicalGradient& a, const SourceBrush::Brush::LogicalGradient& b)
{
    return a.gradient.ptr() == b.gradient.ptr() && a.spaceTransform == b.spaceTransform;
}

inline bool operator!=(const SourceBrush::Brush::LogicalGradient& a, const SourceBrush::Brush::LogicalGradient& b)
{
    return !(a == b);
}

inline bool operator==(const SourceBrush::Brush& a, const SourceBrush::Brush& b)
{
    return WTF::switchOn(a.brush,
        [&] (const SourceBrush::Brush::LogicalGradient& aGradient) {
            if (auto* bGradient = std::get_if<SourceBrush::Brush::LogicalGradient>(&b.brush))
                return aGradient == *bGradient;
            return false;
        },
        [&] (const Ref<Pattern>& aPattern) {
            if (auto* bPattern = std::get_if<Ref<Pattern>>(&b.brush))
                return aPattern.ptr() == bPattern->ptr();
            return false;
        }
    );
}

inline bool operator!=(const SourceBrush::Brush& a, const SourceBrush::Brush& b)
{
    return !(a == b);
}

inline bool operator==(const SourceBrush& a, const SourceBrush& b)
{
    return a.color() == b.color() && a.brush() == b.brush();
}

inline bool operator!=(const SourceBrush& a, const SourceBrush& b)
{
    return !(a == b);
}

template<typename Encoder>
void SourceBrush::Brush::LogicalGradient::encode(Encoder& encoder) const
{
    encoder << gradient.get();
    encoder << spaceTransform;
}

template<typename Decoder>
std::optional<SourceBrush::Brush::LogicalGradient> SourceBrush::Brush::LogicalGradient::decode(Decoder& decoder)
{
    auto gradient = Gradient::decode(decoder);
    if (!gradient)
        return std::nullopt;
    
    std::optional<AffineTransform> spaceTransform;
    decoder >> spaceTransform;
    if (!spaceTransform)
        return std::nullopt;

    return { { WTFMove(*gradient), *spaceTransform } };
}

template<typename Encoder>
void SourceBrush::Brush::encode(Encoder& encoder) const
{
    WTF::switchOn(brush,
        [&] (const LogicalGradient& gradient) {
            encoder << true << gradient;
        },
        [&] (const Ref<Pattern>& pattern) {
            encoder << false << pattern.get();
        }
    );
}

template<typename Decoder>
std::optional<SourceBrush::Brush> SourceBrush::Brush::decode(Decoder& decoder)
{
    std::optional<bool> hasGradient;
    decoder >> hasGradient;
    if (!hasGradient)
        return std::nullopt;

    if (*hasGradient) {
        auto gradient = LogicalGradient::decode(decoder);
        if (!gradient)
            return std::nullopt;
        return { { WTFMove(*gradient) } };
    }

    auto pattern = Pattern::decode(decoder);
    if (!pattern)
        return std::nullopt;
    return { { WTFMove(*pattern) } };
}

template<class Encoder>
void SourceBrush::encode(Encoder& encoder) const
{
    encoder << m_color;
    encoder << m_brush;
}

template<class Decoder>
std::optional<SourceBrush> SourceBrush::decode(Decoder& decoder)
{
    std::optional<Color> color;
    decoder >> color;
    if (!color)
        return std::nullopt;

    std::optional<std::optional<Brush>> brush;
    decoder >> brush;
    if (!brush)
        return std::nullopt;

    return SourceBrush(*color, WTFMove(*brush));
}

WTF::TextStream& operator<<(WTF::TextStream&, const SourceBrush&);

} // namespace WebCore
