/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "TextCodec.h"
#include <wtf/Optional.h>

namespace WebCore {

class TextCodecCJK final : public TextCodec {
public:
    static void registerEncodingNames(EncodingNameRegistrar);
    static void registerCodecs(TextCodecRegistrar);

    enum class Encoding : uint8_t;
    explicit TextCodecCJK(Encoding);

private:
    String decode(const char*, size_t length, bool flush, bool stopOnError, bool& sawError) final;
    Vector<uint8_t> encode(StringView, UnencodableHandling) const final;

    enum class SawError : bool { No, Yes };
    String decodeCommon(const uint8_t*, size_t, bool, bool, bool&, const Function<SawError(uint8_t, StringBuilder&)>&);

    String eucJPDecode(const uint8_t*, size_t, bool, bool, bool&);
    String iso2022JPDecode(const uint8_t*, size_t, bool, bool, bool&);
    String shiftJISDecode(const uint8_t*, size_t, bool, bool, bool&);
    String eucKRDecode(const uint8_t*, size_t, bool, bool, bool&);
    String big5Decode(const uint8_t*, size_t, bool, bool, bool&);

    const Encoding m_encoding;

    bool m_jis0212 { false };

    enum class ISO2022JPDecoderState : uint8_t { ASCII, Roman, Katakana, LeadByte, TrailByte, EscapeStart, Escape };
    ISO2022JPDecoderState m_iso2022JPDecoderState { ISO2022JPDecoderState::ASCII };
    ISO2022JPDecoderState m_iso2022JPDecoderOutputState { ISO2022JPDecoderState::ASCII };
    bool m_iso2022JPOutput { false };
    Optional<uint8_t> m_iso2022JPSecondPrependedByte;

    uint8_t m_lead { 0x00 };
    Optional<uint8_t> m_prependedByte;
};

} // namespace WebCore
