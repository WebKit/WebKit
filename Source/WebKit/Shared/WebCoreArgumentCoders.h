/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include <WebCore/FontPlatformData.h>
#include <wtf/ArgumentCoder.h>
#include <wtf/EnumTraits.h>

#if USE(SKIA)
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include <skia/core/SkColorSpace.h>
#include <skia/core/SkData.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#endif

namespace WebCore {

class Font;
class FontPlatformData;

} // namespace WebCore

namespace IPC {

#if !USE(CORE_TEXT)
template<> struct ArgumentCoder<WebCore::Font> {
    static void encode(Encoder&, const WebCore::Font&);
    static std::optional<Ref<WebCore::Font>> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::Font&);
    static std::optional<WebCore::FontPlatformData> decodePlatformData(Decoder&);
};

template<> struct ArgumentCoder<WebCore::FontPlatformDataAttributes> {
    static void encode(Encoder&, const WebCore::FontPlatformDataAttributes&);
    static std::optional<WebCore::FontPlatformDataAttributes> decode(Decoder&);
    static void encodePlatformData(Encoder&, const WebCore::FontPlatformDataAttributes&);
    static WARN_UNUSED_RETURN bool decodePlatformData(Decoder&, WebCore::FontPlatformDataAttributes&);
};

template<> struct ArgumentCoder<WebCore::FontCustomPlatformData> {
    static void encode(Encoder&, const WebCore::FontCustomPlatformData&);
    static std::optional<Ref<WebCore::FontCustomPlatformData>> decode(Decoder&);
};
#endif

#if USE(SKIA)
template<> struct ArgumentCoder<sk_sp<SkColorSpace>> {
    static void encode(Encoder&, const sk_sp<SkColorSpace>&);
    static void encode(StreamConnectionEncoder&, const sk_sp<SkColorSpace>&);
    static std::optional<sk_sp<SkColorSpace>> decode(Decoder&);
};

template<> struct ArgumentCoder<sk_sp<SkData>> {
    static void encode(Encoder&, const sk_sp<SkData>&);
    static void encode(StreamConnectionEncoder&, const sk_sp<SkData>&);
    static std::optional<sk_sp<SkData>> decode(Decoder&);
};
#endif

} // namespace IPC
