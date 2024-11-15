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

#if USE(SKIA)

#include "CoreIPCSkColorSpace.h"
#include "CoreIPCSkData.h"
#include "StreamConnectionEncoder.h"
#include <WebCore/Font.h>
#include <WebCore/FontCache.h>
#include <WebCore/FontCustomPlatformData.h>

namespace IPC {

void ArgumentCoder<SkString>::encode(Encoder& encoder, const SkString& string)
{
    encoder << std::span { string.data(), string.size() };
}

std::optional<SkString> ArgumentCoder<SkString>::decode(Decoder& decoder)
{
    auto span = decoder.decode<std::span<const char>>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return SkString(span->data(), span->size());
}

void ArgumentCoder<SkFontStyle::Slant>::encode(Encoder& encoder, const SkFontStyle::Slant& slant)
{
    encoder << static_cast<int8_t>(slant);
}

std::optional<SkFontStyle::Slant> ArgumentCoder<SkFontStyle::Slant>::decode(Decoder& decoder)
{
    auto slant = decoder.decode<int8_t>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return static_cast<SkFontStyle::Slant>(*slant);
}

void ArgumentCoder<sk_sp<SkColorSpace>>::encode(Encoder& encoder, const sk_sp<SkColorSpace>& colorSpace)
{
    encoder << WebKit::CoreIPCSkColorSpace(colorSpace);
}

void ArgumentCoder<sk_sp<SkColorSpace>>::encode(StreamConnectionEncoder& encoder, const sk_sp<SkColorSpace>& colorSpace)
{
    encoder << WebKit::CoreIPCSkColorSpace(colorSpace);
}

std::optional<sk_sp<SkColorSpace>> ArgumentCoder<sk_sp<SkColorSpace>>::decode(Decoder& decoder)
{
    auto colorSpace = decoder.decode<WebKit::CoreIPCSkColorSpace>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return colorSpace->skColorSpace();
}

void ArgumentCoder<sk_sp<SkData>>::encode(Encoder& encoder, const sk_sp<SkData>& data)
{
    encoder << WebKit::CoreIPCSkData(data);
}

void ArgumentCoder<sk_sp<SkData>>::encode(StreamConnectionEncoder& encoder, const sk_sp<SkData>& data)
{
    encoder << WebKit::CoreIPCSkData(data);
}

std::optional<sk_sp<SkData>> ArgumentCoder<sk_sp<SkData>>::decode(Decoder& decoder)
{
    auto data = decoder.decode<WebKit::CoreIPCSkData>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    return data->skData();
}

} // namespace IPC

#endif // USE(SKIA)
