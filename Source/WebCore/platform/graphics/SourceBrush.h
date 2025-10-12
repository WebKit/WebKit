/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include <WebCore/Color.h>
#include <WebCore/Gradient.h>
#include <WebCore/Pattern.h>
#include <WebCore/SourceBrushLogicalGradient.h>

namespace WebCore {

class SourceBrush {
public:
    using OptionalPatternGradient = Variant<std::monostate, SourceBrushLogicalGradient, Ref<Pattern>>;

    SourceBrush() = default;
    SourceBrush(const Color& color)
        : m_color(color)
    {
    }

    // Color should be stored in the variant, in place of std::monospace. Currently a lot of code
    // queries the color unconditionally, color is accessible unconditionally, returning incorrect
    // but defined data.
    const Color& color() const { return m_color; }
    void setColor(const Color color) { m_color = color; }
    // Packed color accessor takes into account the discrimination between color, gradient, pattern.
    std::optional<PackedColor::RGBA> packedColor() const;

    WEBCORE_EXPORT Gradient* gradient() const;
    WEBCORE_EXPORT Pattern* pattern() const;
    WEBCORE_EXPORT const AffineTransform& gradientSpaceTransform() const;
    WEBCORE_EXPORT std::optional<RenderingResourceIdentifier> gradientIdentifier() const;

    WEBCORE_EXPORT void setGradient(Ref<Gradient>&&, const AffineTransform& spaceTransform = { });
    WEBCORE_EXPORT void setPattern(Ref<Pattern>&&);

    bool isVisible() const { return hasPatternOrGradient() || m_color.isVisible(); }

    bool hasPatternOrGradient() const { return !std::holds_alternative<std::monostate>(m_patternGradient); }
    friend bool operator==(const SourceBrush&, const SourceBrush&);

private:
    Color m_color { Color::black };
    OptionalPatternGradient m_patternGradient;
};

inline std::optional<PackedColor::RGBA> SourceBrush::packedColor() const
{
    if (hasPatternOrGradient())
        return std::nullopt;
    return m_color.tryGetAsPackedInline();
}

inline bool operator==(const SourceBrush& a, const SourceBrush& b)
{
    // Workaround for Ref<> lack of operator==.
    auto patternGradientEqual = [](const SourceBrush::OptionalPatternGradient& a, const SourceBrush::OptionalPatternGradient& b) -> bool {
        if (a.index() != b.index())
            return false;
        if (auto* aGradient = std::get_if<SourceBrushLogicalGradient>(&a))
            return *aGradient == std::get<SourceBrushLogicalGradient>(b);
        if (auto* aPattern = std::get_if<Ref<Pattern>>(&a))
            return aPattern->ptr() == std::get<Ref<Pattern>>(b).ptr();
        return true;
    };
    return a.m_color == b.m_color && patternGradientEqual(a.m_patternGradient, b.m_patternGradient);
}

WTF::TextStream& operator<<(WTF::TextStream&, const SourceBrush&);

} // namespace WebCore
