/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "NetworkCache.h"

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheDecoder.h"
#include "NetworkCacheEncoder.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(NETWORK_CACHE)

namespace WebKit {
namespace NetworkCache {

Entry::Entry(const Key& key, const WebCore::ResourceResponse& response, RefPtr<WebCore::SharedBuffer>&& buffer, const Vector<std::pair<String, String>>& varyingRequestHeaders)
    : m_key(key)
    , m_timeStamp(std::chrono::system_clock::now())
    , m_response(response)
    , m_varyingRequestHeaders(varyingRequestHeaders)
    , m_buffer(WTF::move(buffer))
{
}

Entry::Entry(const Storage::Record& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
    , m_sourceStorageRecord(storageEntry)
{
}

Storage::Record Entry::encodeAsStorageRecord() const
{
    Encoder encoder;
    encoder << m_response;

    bool hasVaryingRequestHeaders = !m_varyingRequestHeaders.isEmpty();
    encoder << hasVaryingRequestHeaders;
    if (hasVaryingRequestHeaders)
        encoder << m_varyingRequestHeaders;

    encoder.encodeChecksum();

    Data header(encoder.buffer(), encoder.bufferSize());
    Data body;
    if (m_buffer)
        body = { reinterpret_cast<const uint8_t*>(m_buffer->data()), m_buffer->size() };

    return { m_key, m_timeStamp, header, body };
}

std::unique_ptr<Entry> Entry::decodeStorageRecord(const Storage::Record& storageEntry)
{
    std::unique_ptr<Entry> entry(new Entry(storageEntry));

    Decoder decoder(storageEntry.header.data(), storageEntry.header.size());
    if (!decoder.decode(entry->m_response))
        return nullptr;
    entry->m_response.setSource(WebCore::ResourceResponse::Source::DiskCache);

    bool hasVaryingRequestHeaders;
    if (!decoder.decode(hasVaryingRequestHeaders))
        return nullptr;

    if (hasVaryingRequestHeaders) {
        if (!decoder.decode(entry->m_varyingRequestHeaders))
            return nullptr;
    }

    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        return nullptr;
    }

    return entry;
}

#if ENABLE(SHAREABLE_RESOURCE)
void Entry::initializeShareableResourceHandleFromStorageRecord() const
{
    RefPtr<SharedMemory> sharedMemory = m_sourceStorageRecord.body.tryCreateSharedMemory();
    if (!sharedMemory)
        return;

    RefPtr<ShareableResource> shareableResource = ShareableResource::create(sharedMemory.release(), 0, m_sourceStorageRecord.body.size());
    ASSERT(shareableResource);
    shareableResource->createHandle(m_shareableResourceHandle);
}
#endif

void Entry::initializeBufferFromStorageRecord() const
{
#if ENABLE(SHAREABLE_RESOURCE)
    if (!shareableResourceHandle().isNull()) {
        m_buffer = m_shareableResourceHandle.tryWrapInSharedBuffer();
        if (m_buffer)
            return;
    }
#endif
    m_buffer = WebCore::SharedBuffer::create(m_sourceStorageRecord.body.data(), m_sourceStorageRecord.body.size());
}

WebCore::SharedBuffer* Entry::buffer() const
{
    if (!m_buffer)
        initializeBufferFromStorageRecord();

    return m_buffer.get();
}

#if ENABLE(SHAREABLE_RESOURCE)
ShareableResource::Handle& Entry::shareableResourceHandle() const
{
    if (m_shareableResourceHandle.isNull())
        initializeShareableResourceHandleFromStorageRecord();

    return m_shareableResourceHandle;
}
#endif

bool Entry::needsValidation() const
{
    return m_response.source() == WebCore::ResourceResponse::Source::DiskCacheAfterValidation;
}

void Entry::setNeedsValidation()
{
    ASSERT(m_response.source() == WebCore::ResourceResponse::Source::DiskCache);
    m_response.setSource(WebCore::ResourceResponse::Source::DiskCacheAfterValidation);
}

void Entry::asJSON(StringBuilder& json, const Storage::RecordInfo& info) const
{
    json.appendLiteral("{\n");
    json.appendLiteral("\"hash\": ");
    json.appendQuotedJSONString(m_key.hashAsString());
    json.appendLiteral(",\n");
    json.appendLiteral("\"bodySize\": ");
    json.appendNumber(info.bodySize);
    json.appendLiteral(",\n");
    json.appendLiteral("\"worth\": ");
    json.appendNumber(info.worth);
    json.appendLiteral(",\n");
    json.appendLiteral("\"partition\": ");
    json.appendQuotedJSONString(m_key.partition());
    json.appendLiteral(",\n");
    json.appendLiteral("\"timestamp\": ");
    json.appendNumber(std::chrono::duration_cast<std::chrono::milliseconds>(m_timeStamp.time_since_epoch()).count());
    json.appendLiteral(",\n");
    json.appendLiteral("\"URL\": ");
    json.appendQuotedJSONString(m_response.url().string());
    json.appendLiteral(",\n");
    json.appendLiteral("\"bodyHash\": ");
    json.appendQuotedJSONString(info.bodyHash);
    json.appendLiteral(",\n");
    json.appendLiteral("\"bodyShareCount\": ");
    json.appendNumber(info.bodyShareCount);
    json.appendLiteral(",\n");
    json.appendLiteral("\"headers\": {\n");
    bool firstHeader = true;
    for (auto& header : m_response.httpHeaderFields()) {
        if (!firstHeader)
            json.appendLiteral(",\n");
        firstHeader = false;
        json.appendLiteral("    ");
        json.appendQuotedJSONString(header.key);
        json.appendLiteral(": ");
        json.appendQuotedJSONString(header.value);
    }
    json.appendLiteral("\n}\n");
    json.appendLiteral("}");
}

}
}

#endif
