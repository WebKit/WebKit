/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "WebCoreArgumentCoders.h"

#if USE(FREETYPE)

#include <WebCore/Font.h>
#include <WebCore/FontCustomPlatformData.h>

namespace IPC {

void ArgumentCoder<WebCore::Font>::encodePlatformData(Encoder& encoder, const WebCore::Font& font)
{
   // __debugbreak();
    const auto& platformData = font.platformData();
    encoder << platformData.size();
    encoder << platformData.isFixedWidth();
    encoder << platformData.syntheticBold();
    encoder << platformData.syntheticOblique();
    encoder << platformData.orientation();

    const auto& customPlatformData = platformData.customPlatformData();
    encoder << static_cast<bool>(customPlatformData);
    if (customPlatformData)
        encoder << *customPlatformData;
}

std::optional<WebCore::FontPlatformData> ArgumentCoder<WebCore::Font>::decodePlatformData(Decoder& decoder)
{
    //__debugbreak();
    std::optional<float> size;
    decoder >> size;
    if (!size)
        return std::nullopt;

    std::optional<bool> fixedWidth;
    decoder >> fixedWidth;
    if (!fixedWidth)
        return std::nullopt;

    std::optional<bool> syntheticBold;
    decoder >> syntheticBold;
    if (!syntheticBold)
        return std::nullopt;

    std::optional<bool> syntheticOblique;
    decoder >> syntheticOblique;
    if (!syntheticOblique)
        return std::nullopt;

    std::optional<WebCore::FontOrientation> orientation;
    decoder >> orientation;
    if (!orientation)
        return std::nullopt;

    std::optional<bool> includesCreationData;
    decoder >> includesCreationData;
    if (!includesCreationData)
        return std::nullopt;

    // Currently creation data is always required
    if (*includesCreationData)
        return std::nullopt;

    std::optional<Ref<WebCore::FontCustomPlatformData>> fontCustomPlatformData;
    decoder >> fontCustomPlatformData;
    if (!fontCustomPlatformData)
        return std::nullopt;

    // Need FCPattern

    return WebCore::FontPlatformData((*fontCustomPlatformData)->m_fontFace.get(), nullptr, *size, *fixedWidth, *syntheticBold, *syntheticOblique, *orientation, (*fontCustomPlatformData).ptr());
}

void ArgumentCoder<WebCore::FontPlatformData::Attributes>::encodePlatformData(Encoder&, const WebCore::FontPlatformData::Attributes&)
{
    // No FreeType specific fields on Attributes
}

bool ArgumentCoder<WebCore::FontPlatformData::Attributes>::decodePlatformData(Decoder&, WebCore::FontPlatformData::Attributes&)
{
    // No FreeType specific fields on Attributes
    return true;
}

} // namespace IPC

#endif // USE(FREETYPE)
