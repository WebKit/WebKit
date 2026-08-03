/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "Helpers/Test.h"
#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/InitializeThreading.h>
#include <JavaScriptCore/Uint8Array.h>
#include <WebCore/TextEncoderStreamEncoder.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

// The two halves of U+1F499 BLUE HEART, which is not in the Basic Multilingual Plane.
constexpr char16_t leadSurrogate = 0xD83D;
constexpr char16_t trailSurrogate = 0xDC99;

constexpr std::array<uint8_t, 4> astralCharacterEncoded { 0xF0, 0x9F, 0x92, 0x99 };
constexpr std::array<uint8_t, 3> replacementCharacterEncoded { 0xEF, 0xBF, 0xBD };

static String stringFromCodeUnit(char16_t codeUnit)
{
    std::array<char16_t, 1> codeUnits { codeUnit };
    return String(std::span<const char16_t> { codeUnits });
}

static Vector<uint8_t> bytes(RefPtr<Uint8Array>&& array)
{
    if (!array)
        return { };
    return Vector<uint8_t>(array->typedSpan());
}

static Ref<WebCore::TextEncoderStreamEncoder> createEncoder()
{
    JSC::initialize();
    return WebCore::TextEncoderStreamEncoder::create();
}

TEST(TextEncoderStreamEncoder, EncodeSurrogatePairSplitAcrossChunks)
{
    auto encoder = createEncoder();
    EXPECT_NULL(encoder->encode(stringFromCodeUnit(leadSurrogate)));
    EXPECT_EQ(bytes(encoder->encode(stringFromCodeUnit(trailSurrogate))), Vector<uint8_t>(astralCharacterEncoded));
    EXPECT_NULL(encoder->flush());
}

TEST(TextEncoderStreamEncoder, FlushEmitsReplacementCharacterForPendingLeadSurrogate)
{
    auto encoder = createEncoder();
    EXPECT_NULL(encoder->encode(stringFromCodeUnit(leadSurrogate)));
    EXPECT_EQ(bytes(encoder->flush()), Vector<uint8_t>(replacementCharacterEncoded));
}

TEST(TextEncoderStreamEncoder, FlushClearsPendingLeadSurrogate)
{
    auto encoder = createEncoder();
    EXPECT_NULL(encoder->encode(stringFromCodeUnit(leadSurrogate)));
    EXPECT_EQ(bytes(encoder->flush()), Vector<uint8_t>(replacementCharacterEncoded));

    // The pending lead surrogate was consumed by the first flush, so flushing
    // again must not emit a second replacement character.
    EXPECT_NULL(encoder->flush());
}

TEST(TextEncoderStreamEncoder, EncodeAfterFlushStartsFromCleanState)
{
    auto encoder = createEncoder();
    EXPECT_NULL(encoder->encode(stringFromCodeUnit(leadSurrogate)));
    EXPECT_EQ(bytes(encoder->flush()), Vector<uint8_t>(replacementCharacterEncoded));

    // The lead surrogate was already replaced by the flush above, so the trail
    // surrogate is unpaired and must be replaced too rather than combined into
    // an astral character.
    EXPECT_EQ(bytes(encoder->encode(stringFromCodeUnit(trailSurrogate))), Vector<uint8_t>(replacementCharacterEncoded));
}

} // namespace TestWebKitAPI
