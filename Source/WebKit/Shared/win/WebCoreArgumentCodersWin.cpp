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

void ArgumentCoder<Ref<Font>>::encodePlatformData(Encoder& encoder, const Ref<Font>& font)
{
    const auto& platformData = font->platformData();
    encoder << platformData.orientation();
    encoder << platformData.widthVariant();
    encoder << platformData.textRenderingMode();
    encoder << platformData.size();
    encoder << platformData.syntheticBold();
    encoder << platformData.syntheticOblique();
    encoder << platformData.familyName();
}

Optional<FontPlatformData> ArgumentCoder<Ref<Font>>::decodePlatformData(Decoder& decoder)
{
    Optional<FontOrientation> orientation;
    decoder >> orientation;
    if (!orientation.hasValue())
        return WTF::nullopt;

    Optional<FontWidthVariant> widthVariant;
    decoder >> widthVariant;
    if (!widthVariant.hasValue())
        return WTF::nullopt;

    Optional<TextRenderingMode> textRenderingMode;
    decoder >> textRenderingMode;
    if (!textRenderingMode.hasValue())
        return WTF::nullopt;

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

    Optional<String> familyName;
    decoder >> familyName;
    if (!familyName.hasValue())
        return WTF::nullopt;

    FontDescription description;
    description.setOrientation(*orientation);
    description.setWidthVariant(*widthVariant);
    description.setTextRenderingMode(*textRenderingMode);
    description.setComputedSize(*size);
    RefPtr<Font> font = FontCache::singleton().fontForFamily(description, *familyName);
    if (!font)
        return WTF::nullopt;
    
    LOGFONT logFont;
    GetObject(font->platformData().hfont(), sizeof logFont, &logFont);
    auto gdiFont = adoptGDIObject(CreateFontIndirect(&logFont));
    if (!gdiFont)
        return WTF::nullopt;
    
    return FontPlatformData(WTFMove(gdiFont), *size, *syntheticBold, *syntheticOblique, false);
}

} // namespace IPC
