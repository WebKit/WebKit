/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
#include "NetworkCacheCompressionDictionaryEntry.h"

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include <WebCore/SharedBuffer.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/HexNumber.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {
// Created by binding generator.
String convertEnumerationToString(FetchOptions::Destination);
}

namespace WebKit::NetworkCache {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CompressionDictionaryEntry);

std::optional<WebCore::FetchOptions::Destination> parseFetchDestination(const String& string)
{
    std::optional<WebCore::FetchOptions::Destination> result;
    WTF::EnumTraitsForPersistence<WebCore::FetchOptions::Destination>::values::forEach([&](auto destination) {
        if (!result && WebCore::convertEnumerationToString(destination) == string)
            result = destination;
    });
    return result;
}

CompressionDictionaryEntry::CompressionDictionaryEntry(const Key& key, Info&& info, RefPtr<WebCore::FragmentedSharedBuffer>&& buffer)
    : m_key(key)
    , m_timeStamp(WallTime::now())
    , m_info(WTF::move(info))
    , m_buffer(WTF::move(buffer))
{
    auto digest = PAL::Crypto::CryptoDigest::create(PAL::Crypto::CryptoDigest::Algorithm::SHA_256);
    if (RefPtr buffer = m_buffer) {
        buffer->forEachSegment([&](auto segment) {
            digest->addBytes(segment);
        });
    }
    memcpySpan(std::span { m_hash }, digest->computeHash().span());
}

CompressionDictionaryEntry::CompressionDictionaryEntry(const Storage::Record& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
{
}

Storage::Record CompressionDictionaryEntry::encodeAsStorageRecord() const
{
    WTF::Persistence::Encoder encoder;
    encoder << version;
    encoder << m_info.match;
    encoder << m_info.id;
    encoder << m_info.matchDest.toRaw();
    encoder << m_info.expirationTime;
    encoder.encodeFixedLengthData(m_hash);

    encoder.encodeChecksum();

    Data header(encoder.span());
    Data body;
    if (RefPtr buffer = m_buffer) {
        Ref contiguousBuffer = buffer->makeContiguous();
        m_buffer = contiguousBuffer.copyRef();
        body = { contiguousBuffer->span() };
    }

    return { m_key, m_timeStamp, header, body, { } };
}

std::unique_ptr<CompressionDictionaryEntry> CompressionDictionaryEntry::decodeStorageRecord(const Storage::Record& storageEntry)
{
    auto entry = makeUnique<CompressionDictionaryEntry>(storageEntry);

    WTF::Persistence::Decoder decoder(storageEntry.header.span());

    std::optional<uint8_t> recordVersion;
    decoder >> recordVersion;
    if (recordVersion != version)
        return nullptr;

    std::optional<String> match;
    decoder >> match;
    if (!match)
        return nullptr;

    std::optional<String> id;
    decoder >> id;
    if (!id)
        return nullptr;

    std::optional<uint32_t> matchDest;
    decoder >> matchDest;
    if (!matchDest)
        return nullptr;

    std::optional<WallTime> expirationTime;
    decoder >> expirationTime;
    if (!expirationTime)
        return nullptr;

    std::array<uint8_t, hashSize> hash { };
    if (!decoder.decodeFixedLengthData(hash))
        return nullptr;

    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) compression dictionary checksum verification failure\n");
        return nullptr;
    }

    entry->m_info = Info { WTF::move(*match), WTF::move(*id), DestinationSet::fromRaw(*matchDest), *expirationTime };
    entry->m_hash = hash;

    return entry;
}

void CompressionDictionaryEntry::asJSON(StringBuilder& json, const Storage::RecordInfo& info) const
{
    json.append("{\n"
        "\"hash\": "_s);
    json.appendQuotedJSONString(m_key.hashAsString());
    json.append(",\n"
        "\"bodySize\": "_s, info.bodySize,
        ",\n"
        "\"worth\": "_s, info.worth,
        ",\n"
        "\"partition\": "_s);
    json.appendQuotedJSONString(m_key.partition());
    json.append(",\n"
        "\"timestamp\": "_s, m_timeStamp.secondsSinceEpoch().milliseconds(),
        ",\n"
        "\"URL\": "_s);
    json.appendQuotedJSONString(m_key.identifier());
    json.append(",\n"
        "\"bodyShareCount\": "_s, info.bodyShareCount,
        ",\n"
        "\"match\": "_s);
    json.appendQuotedJSONString(m_info.match);
    json.append(",\n"
        "\"id\": "_s);
    json.appendQuotedJSONString(m_info.id);
    json.append(",\n"
        "\"dictionaryHash\": "_s);
    json.appendQuotedJSONString(toHexString(m_hash));
    json.append(",\n"
        "\"expirationTime\": "_s, m_info.expirationTime.secondsSinceEpoch().milliseconds(),
        ",\n"
        "\"matchDest\": [\n"_s);
    bool firstDestination = true;
    WTF::EnumTraitsForPersistence<WebCore::FetchOptions::Destination>::values::forEach([&](auto destination) {
        if (!m_info.matchDest.contains(destination))
            return;
        json.append(std::exchange(firstDestination, false) ? ""_s : ",\n"_s, "    "_s);
        json.appendQuotedJSONString(WebCore::convertEnumerationToString(destination));
    });
    json.append("\n"
        "]\n"
        "}"_s);
}

}
