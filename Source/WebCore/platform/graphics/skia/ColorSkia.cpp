/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "Color.h"

#if USE(SKIA)

namespace WebCore {

Color::Color(const SkColor& skColor)
    : Color(SRGBA<uint8_t> { static_cast<uint8_t>(SkColorGetR(skColor)), static_cast<uint8_t>(SkColorGetG(skColor)), static_cast<uint8_t>(SkColorGetB(skColor)), static_cast<uint8_t>(SkColorGetA(skColor)) })
{
}

Color::operator SkColor() const
{
    auto [r, g, b, a] = toColorTypeLossy<SRGBA<uint8_t>>().resolved();
    return SkColorSetARGB(a, r, g, b);
}

Color::Color(const SkColor4f& skColor)
    : Color(convertColor<SRGBA<uint8_t>>(SRGBA<float> { skColor.fR, skColor.fG, skColor.fB, skColor.fA }))
{
}

Color::operator SkColor4f() const
{
    auto [r, g, b, a] = toColorTypeLossy<SRGBA<float>>().resolved();
    return { r, g, b, a };
}

} // namespace WebCore

#endif // USE(SKIA)

