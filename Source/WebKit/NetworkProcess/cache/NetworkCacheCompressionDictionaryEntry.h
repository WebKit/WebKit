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

#pragma once

#include "NetworkCacheStorage.h"
#include <WebCore/FetchOptions.h>
#include <wtf/Noncopyable.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class FragmentedSharedBuffer;
}

namespace WebKit::NetworkCache {

// A dictionary registered by Use-As-Dictionary, stored beside the regular cache entry for the
// same URL. https://www.rfc-editor.org/rfc/rfc9842#name-use-as-dictionary
//
// A dictionary is found by matching its pattern against an outbound request rather than by
// retrieving its own URL, so the record holds neither the response it arrived with nor anything
// else that only applies to a resource retrieved by a request.
class CompressionDictionaryEntry {
    WTF_MAKE_TZONE_ALLOCATED(CompressionDictionaryEntry);
    WTF_MAKE_NONCOPYABLE(CompressionDictionaryEntry);
public:
    // Incrementing this discards only the stored dictionaries, not the whole cache like Storage::version does.
    static constexpr uint8_t version = 1;
    static constexpr size_t hashSize = 32;

    // FetchOptions::Destination values are not powers of two, so OptionSet cannot hold them.
    class DestinationSet {
    public:
        constexpr DestinationSet() = default;
        static constexpr DestinationSet fromRaw(uint32_t bits) { return DestinationSet { bits }; }
        constexpr uint32_t toRaw() const { return m_bits; }

        constexpr bool isEmpty() const { return !m_bits; }
        constexpr bool contains(WebCore::FetchOptions::Destination destination) const { return m_bits & bit(destination); }
        constexpr void add(WebCore::FetchOptions::Destination destination) { m_bits |= bit(destination); }

    private:
        constexpr explicit DestinationSet(uint32_t bits)
            : m_bits(bits)
        {
        }

        static constexpr uint32_t bit(WebCore::FetchOptions::Destination destination) { return 1u << std::to_underlying(destination); }
        static_assert(std::to_underlying(WTF::EnumTraitsForPersistence<WebCore::FetchOptions::Destination>::values::max) < 32);

        uint32_t m_bits { 0 };
    };

    struct Info {
        String match;
        String id;
        DestinationSet matchDest;
        WallTime expirationTime;
    };

    CompressionDictionaryEntry(const Key&, Info&&, RefPtr<WebCore::FragmentedSharedBuffer>&&);
    explicit CompressionDictionaryEntry(const Storage::Record&);

    Storage::Record encodeAsStorageRecord() const;
    static std::unique_ptr<CompressionDictionaryEntry> decodeStorageRecord(const Storage::Record&);

    void asJSON(StringBuilder&, const Storage::RecordInfo&) const;

private:
    Key m_key;
    WallTime m_timeStamp;
    Info m_info;
    std::array<uint8_t, hashSize> m_hash { };
    mutable RefPtr<WebCore::FragmentedSharedBuffer> m_buffer;
};

std::optional<WebCore::FetchOptions::Destination> parseFetchDestination(const String&);

}
