/*
 * Copyright (C) 2022-2023 Apple Inc.  All rights reserved.
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
#include "SourceBrushLogicalGradient.h"

namespace WebCore {

class SourceBrush {
public:
    struct Brush {
        using LogicalGradient = SourceBrushLogicalGradient;

        friend bool operator==(const Brush&, const Brush&);

        using Variant = std::variant<LogicalGradient, Ref<Pattern>>;
        Variant brush;
    };

    SourceBrush() = default;
    WEBCORE_EXPORT SourceBrush(const Color&, std::optional<Brush>&& = std::nullopt);

    const Color& color() const { return m_color; }
    void setColor(const Color color) { m_color = color; }

    const std::optional<Brush>& brush() const { return m_brush; }

    WEBCORE_EXPORT Gradient* gradient() const;
    WEBCORE_EXPORT Pattern* pattern() const;
    WEBCORE_EXPORT const AffineTransform& gradientSpaceTransform() const;
    WEBCORE_EXPORT std::optional<RenderingResourceIdentifier> gradientIdentifier() const;

    WEBCORE_EXPORT void setGradient(Ref<Gradient>&&, const AffineTransform& spaceTransform = { });
    void setPattern(Ref<Pattern>&&);

    bool isInlineColor() const { return !m_brush && m_color.tryGetAsSRGBABytes(); }
    bool isVisible() const { return m_brush || m_color.isVisible(); }

    friend bool operator==(const SourceBrush&, const SourceBrush&) = default;

private:
    Color m_color { Color::black };
    std::optional<Brush> m_brush;
};

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

WTF::TextStream& operator<<(WTF::TextStream&, const SourceBrush&);

} // namespace WebCore
