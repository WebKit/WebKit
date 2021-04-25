/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include <WebCore/Font.h>
#include <WebCore/FontAttributes.h>
#include <WebCore/FontCache.h>
#include <WebCore/FontCustomPlatformData.h>
#include <WebCore/FontDescription.h>
#include <wtf/win/GDIObject.h>

namespace IPC {

using namespace WebCore;

void ArgumentCoder<FontAttributes>::encodePlatformData(Encoder&, const FontAttributes&)
{
    ASSERT_NOT_REACHED();
}

Optional<FontAttributes> ArgumentCoder<FontAttributes>::decodePlatformData(Decoder&, FontAttributes&)
{
    ASSERT_NOT_REACHED();
    return WTF::nullopt;
}

template<> struct ArgumentCoder<LOGFONT> {
    static void encode(Encoder& encoder, const LOGFONT& logFont)
    {
        encoder.encodeFixedLengthData(reinterpret_cast<const uint8_t*>(&logFont), sizeof logFont, 1);
    }
    static Optional<LOGFONT> decode(Decoder& decoder)
    {
        LOGFONT logFont;
        if (!decoder.decodeFixedLengthData(reinterpret_cast<uint8_t*>(&logFont), sizeof(logFont), 1))
            return WTF::nullopt;
        return logFont;
    }
};

void ArgumentCoder<Ref<Font>>::encodePlatformData(Encoder& encoder, const Ref<Font>& font)
{
    const auto& platformData = font->platformData();
    encoder << platformData.size();
    encoder << platformData.syntheticBold();
    encoder << platformData.syntheticOblique();

    const auto& creationData = platformData.creationData();
    encoder << static_cast<bool>(creationData);
    if (creationData) {
        encoder << creationData->fontFaceData;
        encoder << creationData->itemInCollection;
    }

    LOGFONT logFont;
    GetObject(platformData.hfont(), sizeof logFont, &logFont);
    encoder << logFont;
}

Optional<FontPlatformData> ArgumentCoder<Ref<Font>>::decodePlatformData(Decoder& decoder)
{
    Optional<float> size;
    decoder >> size;
    if (!size.hasValue())
        return WTF::nullopt;

    Optional<bool> syntheticBold;
    decoder >> syntheticBold;
    if (!syntheticBold.hasValue())
        return WTF::nullopt;

    Optional<bool> syntheticOblique;
    decoder >> syntheticOblique;
    if (!syntheticOblique.hasValue())
        return WTF::nullopt;

    Optional<bool> includesCreationData;
    decoder >> includesCreationData;
    if (!includesCreationData.hasValue())
        return WTF::nullopt;

    std::unique_ptr<FontCustomPlatformData> fontCustomPlatformData;
    FontPlatformData::CreationData* creationData = nullptr;

    if (includesCreationData.value()) {
        Optional<Ref<SharedBuffer>> fontFaceData;
        decoder >> fontFaceData;
        if (!fontFaceData.hasValue())
            return WTF::nullopt;

        Optional<String> itemInCollection;
        decoder >> itemInCollection;
        if (!itemInCollection.hasValue())
            return WTF::nullopt;

        fontCustomPlatformData = createFontCustomPlatformData(fontFaceData.value(), itemInCollection.value());
        if (!fontCustomPlatformData)
            return WTF::nullopt;
        creationData = &fontCustomPlatformData->creationData;
    }

    Optional<LOGFONT> logFont;
    decoder >> logFont;
    if (!logFont.hasValue())
        return WTF::nullopt;

    if (fontCustomPlatformData)
        wcscpy_s(logFont->lfFaceName, LF_FACESIZE, fontCustomPlatformData->name.wideCharacters().data());
    
    auto gdiFont = adoptGDIObject(CreateFontIndirect(&*logFont));
    if (!gdiFont)
        return WTF::nullopt;

    return FontPlatformData(WTFMove(gdiFont), *size, *syntheticBold, *syntheticOblique, false, creationData);
}

} // namespace IPC
