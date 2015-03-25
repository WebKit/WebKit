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
#include <JavaScriptCore/JSONObject.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(NETWORK_CACHE)

namespace WebKit {
namespace NetworkCache {

Entry::Entry(const Key& key, const WebCore::ResourceResponse& response, RefPtr<WebCore::SharedBuffer>&& buffer, const Vector<std::pair<String, String>>& varyingRequestHeaders)
    : m_key(key)
    , m_timeStamp(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()))
    , m_response(response)
    , m_varyingRequestHeaders(varyingRequestHeaders)
    , m_buffer(WTF::move(buffer))
{
}

Entry::Entry(const Storage::Entry& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
    , m_sourceStorageEntry(storageEntry)
{
}

Storage::Entry Entry::encode() const
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

std::unique_ptr<Entry> Entry::decode(const Storage::Entry& storageEntry)
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

void Entry::initializeBufferFromStorageEntry() const
{
    auto* data = m_sourceStorageEntry.body.data();
    size_t size = m_sourceStorageEntry.body.size();
#if ENABLE(SHAREABLE_RESOURCE)
    RefPtr<SharedMemory> sharedMemory = m_sourceStorageEntry.body.isMap() ? SharedMemory::createFromVMBuffer(const_cast<uint8_t*>(data), size) : nullptr;
    RefPtr<ShareableResource> shareableResource = sharedMemory ? ShareableResource::create(sharedMemory.release(), 0, m_sourceStorageEntry.body.size()) : nullptr;

    if (shareableResource && shareableResource->createHandle(m_shareableResourceHandle))
        m_buffer = m_shareableResourceHandle.tryWrapInSharedBuffer();
#endif
    if (!m_buffer)
        m_buffer = WebCore::SharedBuffer::create(data, size);
}

WebCore::SharedBuffer* Entry::buffer() const
{
    if (!m_buffer)
        initializeBufferFromStorageEntry();

    return m_buffer.get();
}

ShareableResource::Handle& Entry::shareableResourceHandle() const
{
    if (!m_buffer)
        initializeBufferFromStorageEntry();

    return m_shareableResourceHandle;
}

bool Entry::needsValidation() const
{
    return m_response.source() == WebCore::ResourceResponse::Source::DiskCacheAfterValidation;
}

void Entry::setNeedsValidation()
{
    ASSERT(m_response.source() == WebCore::ResourceResponse::Source::DiskCache);
    m_response.setSource(WebCore::ResourceResponse::Source::DiskCacheAfterValidation);
}

void Entry::asJSON(StringBuilder& json) const
{
    json.appendLiteral("{\n");
    json.appendLiteral("\"hash\": ");
    JSC::appendQuotedJSONStringToBuilder(json, m_key.hashAsString());
    json.appendLiteral(",\n");
    json.appendLiteral("\"partition\": ");
    JSC::appendQuotedJSONStringToBuilder(json, m_key.partition());
    json.appendLiteral(",\n");
    json.appendLiteral("\"timestamp\": ");
    json.appendNumber(m_timeStamp.count());
    json.appendLiteral(",\n");
    json.appendLiteral("\"URL\": ");
    JSC::appendQuotedJSONStringToBuilder(json, m_response.url().string());
    json.appendLiteral(",\n");
    json.appendLiteral("\"headers\": {\n");
    bool firstHeader = true;
    for (auto& header : m_response.httpHeaderFields()) {
        if (!firstHeader)
            json.appendLiteral(",\n");
        firstHeader = false;
        json.appendLiteral("    ");
        JSC::appendQuotedJSONStringToBuilder(json, header.key);
        json.appendLiteral(": ");
        JSC::appendQuotedJSONStringToBuilder(json, header.value);
    }
    json.appendLiteral("\n}\n");
    json.appendLiteral("}");
}

}
}

#endif
