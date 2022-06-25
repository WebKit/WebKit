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

#include "config.h"
#include "SourceBrush.h"

namespace WebCore {

SourceBrush::SourceBrush(const Color& color, std::optional<Brush>&& brush)
    : m_color(color)
    , m_brush(WTFMove(brush))
{
}

const AffineTransform& SourceBrush::gradientSpaceTransform() const
{
    static NeverDestroyed<AffineTransform> identity;
    if (!m_brush)
        return identity.get();

    if (auto* gradient = std::get_if<Brush::LogicalGradient>(&m_brush->brush))
        return gradient->spaceTransform;

    return identity.get();
}

Gradient* SourceBrush::gradient() const
{
    if (!m_brush)
        return nullptr;
    if (auto* gradient = std::get_if<Brush::LogicalGradient>(&m_brush->brush))
        return gradient->gradient.ptr();
    return nullptr;
}

Pattern* SourceBrush::pattern() const
{
    if (!m_brush)
        return nullptr;
    if (auto* pattern = std::get_if<Ref<Pattern>>(&m_brush->brush))
        return pattern->ptr();
    return nullptr;
}

void SourceBrush::setGradient(Ref<Gradient>&& gradient, const AffineTransform& spaceTransform)
{
    m_brush = { Brush::LogicalGradient { WTFMove(gradient), spaceTransform } };
}

void SourceBrush::setPattern(Ref<Pattern>&& pattern)
{
    m_brush = { WTFMove(pattern) };
}

WTF::TextStream& operator<<(TextStream& ts, const SourceBrush& brush)
{
    ts.dumpProperty("color", brush.color());

    if (auto gradient = brush.gradient()) {
        ts.dumpProperty("gradient", *gradient);
        ts.dumpProperty("gradient-space-transform", brush.gradientSpaceTransform());
    }

    if (auto pattern = brush.pattern())
        ts.dumpProperty("pattern", pattern);

    return ts;
}

} // namespace WebCore
