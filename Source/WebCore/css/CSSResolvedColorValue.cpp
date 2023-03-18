/**
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "CSSResolvedColorValue.h"

#include "ColorSerialization.h"

namespace WebCore {

CSSResolvedColorValue::CSSResolvedColorValue(Color&& color)
    : CSSValue(Type::Color)
    , m_color(WTFMove(color))
{
}

Ref<CSSResolvedColorValue> CSSResolvedColorValue::create(Color&& color)
{
    if (auto result = tryCreateScalar(color))
        return result.releaseNonNull();
    return adoptRef(*new CSSResolvedColorValue(WTFMove(color)));
}

RefPtr<CSSResolvedColorValue> CSSResolvedColorValue::tryCreateScalar(const Color& color)
{
    auto bytes = color.tryGetAsSRGBABytes();
    if (!bytes)
        return nullptr;
    uintptr_t payload = PackedColor::RGBA { *bytes }.value;
    if constexpr (sizeof(uintptr_t) < 8) {
        // Check that we have 24 bits to work with.
        static_assert(std::numeric_limits<uintptr_t>::max() >> PayloadShift >= 0xFFFFFF);
        // Check that reusing the 2 high bits as the 2 low bits of each channel doesn't change the color.
        if ((payload >> 6 & 0x03030303) != (payload & 0x03030303))
            return nullptr;
        payload = (payload >> 26 & 0x3F) << 18 | (payload >> 18 & 0x3F) << 12 | (payload >> 10 & 0x3F) << 6 | (payload >> 2 & 0x3F);
    }
    return reinterpret_cast<CSSResolvedColorValue*>(typeScalar(Type::Color) | payload << PayloadShift);
}

Color CSSResolvedColorValue::color() const
{
    if (hasScalarInPointer()) {
        uint32_t payload = scalar() >> PayloadShift;
        if constexpr (sizeof(uintptr_t) >= 8)
            return asSRGBA(PackedColor::RGBA { payload });
        else {
            uint8_t red = payload >> 16 & 0xFC;
            uint8_t green = payload >> 10 & 0xFC;
            uint8_t blue = payload >> 4 & 0xFC;
            uint8_t alpha = payload << 2 & 0xFC;
            red |= red >> 6;
            green |= green >> 6;
            blue |= blue >> 6;
            alpha |= alpha >> 6;
            return SRGBA<uint8_t> { red, green, blue, alpha };
        }
    }
    return opaque(this)->m_color;
}

String CSSResolvedColorValue::customCSSText() const
{
    return serializationForCSS(color());
}

} // namespace WebCore
