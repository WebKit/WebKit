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

#include <WebCore/ResourceResponse.h>
#include <WebCore/WebCorePersistentCoders.h>
#include <array>
#include <bit>
#include <wtf/URL.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>

namespace TestWebKitAPI {

using WebCore::ResourceResponseData;

struct PersistedFlag {
    ASCIILiteral fieldName;
    void (*set)(ResourceResponseData&);
    bool (*isSet)(const ResourceResponseData&);
};

// Adding a flag to the coder means adding a row here; nothing else in this file depends on the encoded layout.
static const std::array persistedFlags {
    PersistedFlag { "isRedirected"_s,
        [](auto& data) { data.isRedirected = true; },
        [](auto& data) { return data.isRedirected; } },
    PersistedFlag { "usedLegacyTLS"_s,
        [](auto& data) { data.usedLegacyTLS = WebCore::UsedLegacyTLS::Yes; },
        [](auto& data) { return data.usedLegacyTLS == WebCore::UsedLegacyTLS::Yes; } },
    PersistedFlag { "wasPrivateRelayed"_s,
        [](auto& data) { data.wasPrivateRelayed = WebCore::WasPrivateRelayed::Yes; },
        [](auto& data) { return data.wasPrivateRelayed == WebCore::WasPrivateRelayed::Yes; } },
    PersistedFlag { "isRangeRequested"_s,
        [](auto& data) { data.isRangeRequested = true; },
        [](auto& data) { return data.isRangeRequested; } },
    PersistedFlag { "requestIncludesCredentials"_s,
        [](auto& data) { data.requestIncludesCredentials = WebCore::IncludesCredentials::Yes; },
        [](auto& data) { return data.requestIncludesCredentials == WebCore::IncludesCredentials::Yes; } },
};

static ResourceResponseData responseDataWithNoFlagsSet()
{
    WebCore::ResourceResponse response { URL { "https://example.com/"_s }, "text/plain"_s, 5, "UTF-8"_s };
    auto data = *response.getResponseData();
    data.isRedirected = false;
    data.usedLegacyTLS = WebCore::UsedLegacyTLS::No;
    data.wasPrivateRelayed = WebCore::WasPrivateRelayed::No;
    data.isRangeRequested = false;
    data.requestIncludesCredentials = WebCore::IncludesCredentials::No;
    return data;
}

static Vector<uint8_t> encode(const ResourceResponseData& data)
{
    WTF::Persistence::Encoder encoder;
    WTF::Persistence::Coder<ResourceResponseData>::encodeForPersistence(encoder, data);
    return { encoder.span() };
}

static std::optional<ResourceResponseData> decode(std::span<const uint8_t> bytes)
{
    WTF::Persistence::Decoder decoder(bytes);
    return WTF::Persistence::Coder<ResourceResponseData>::decodeForPersistence(decoder);
}

// Setting one flag at a time catches two flags sharing or swapping a bit, which setting all of them at once would not.
TEST(ResourceResponseDataPersistence, RoundTripsEachFlagIndependently)
{
    for (auto& flag : persistedFlags) {
        auto data = responseDataWithNoFlagsSet();
        flag.set(data);

        auto decoded = decode(encode(data).span());
        ASSERT_TRUE(decoded.has_value()) << flag.fieldName.characters();
        for (auto& otherFlag : persistedFlags)
            EXPECT_EQ(otherFlag.isSet(*decoded), &otherFlag == &flag) << "set " << flag.fieldName.characters() << ", read " << otherFlag.fieldName.characters();
    }
}

// A newer build may set flag bits this build doesn't know. Records it writes must still decode here, or adding
// a flag would need a version bump. The flags word is located by toggling a known flag rather than by offset,
// so this keeps working as fields are added around it. Bit 31 is the last bit a future field should claim.
TEST(ResourceResponseDataPersistence, IgnoresUnknownFlags)
{
    auto data = responseDataWithNoFlagsSet();
    auto withoutFlags = encode(data);
    persistedFlags[0].set(data);
    auto withBitZero = encode(data);
    ASSERT_EQ(withoutFlags.size(), withBitZero.size());

    auto mismatch = std::ranges::mismatch(withoutFlags, withBitZero);
    ASSERT_NE(mismatch.in1, withoutFlags.end());
    size_t lowByteOffset = mismatch.in1 - withoutFlags.begin();
    size_t highByteOffset;
    if constexpr (std::endian::native == std::endian::little)
        highByteOffset = lowByteOffset + sizeof(uint32_t) - 1;
    else
        highByteOffset = lowByteOffset - (sizeof(uint32_t) - 1);
    ASSERT_LT(highByteOffset, withBitZero.size());

    withBitZero[highByteOffset] |= 0x80;
    auto decoded = decode(withBitZero.span());
    ASSERT_TRUE(decoded.has_value());
    for (auto& flag : persistedFlags)
        EXPECT_EQ(flag.isSet(*decoded), &flag == &persistedFlags[0]) << flag.fieldName.characters();
}

}
