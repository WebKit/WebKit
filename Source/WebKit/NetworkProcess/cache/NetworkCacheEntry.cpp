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
#include "NetworkProcess.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/ResourceRequest.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/persistence/PersistentEncoder.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
namespace NetworkCache {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Entry);

Entry::Entry(const Key& key, const WebCore::ResourceResponse& response, PrivateRelayed privateRelayed, RefPtr<WebCore::FragmentedSharedBuffer>&& buffer, const Vector<std::pair<String, String>>& varyingRequestHeaders)
    : m_key(key)
    , m_timeStamp(WallTime::now())
    , m_response(response)
    , m_varyingRequestHeaders(varyingRequestHeaders)
    , m_buffer(WTFMove(buffer))
    , m_privateRelayed(privateRelayed)
{
    ASSERT(m_key.type() == "Resource"_s);
}

Entry::Entry(const Key& key, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& redirectRequest, const Vector<std::pair<String, String>>& varyingRequestHeaders)
    : m_key(key)
    , m_timeStamp(WallTime::now())
    , m_response(response)
    , m_varyingRequestHeaders(varyingRequestHeaders)
{
    ASSERT(m_key.type() == "Resource"_s);

    m_redirectRequest.emplace();
    m_redirectRequest->setAsIsolatedCopy(redirectRequest);
    // Redirect body is not needed even if exists.
    m_redirectRequest->setHTTPBody(nullptr);
}

Entry::Entry(const Entry& other)
    : m_key(other.m_key)
    , m_timeStamp(other.m_timeStamp)
    , m_response(other.m_response)
    , m_varyingRequestHeaders(other.m_varyingRequestHeaders)
    , m_redirectRequest(other.m_redirectRequest)
    , m_buffer(other.m_buffer)
    , m_sourceStorageRecord(other.m_sourceStorageRecord)
{
}

Entry::Entry(const Storage::Record& storageEntry)
    : m_key(storageEntry.key)
    , m_timeStamp(storageEntry.timeStamp)
    , m_sourceStorageRecord(storageEntry)
{
    ASSERT(m_key.type() == "Resource"_s);
}

Storage::Record Entry::encodeAsStorageRecord() const
{
    WTF::Persistence::Encoder encoder;
    encoder << m_response;

    bool hasVaryingRequestHeaders = !m_varyingRequestHeaders.isEmpty();
    encoder << hasVaryingRequestHeaders;
    if (hasVaryingRequestHeaders)
        encoder << m_varyingRequestHeaders;

    uint8_t isRedirect = !!m_redirectRequest;
    uint8_t privateRelayed = m_privateRelayed == PrivateRelayed::Yes;
    encoder << static_cast<uint8_t>((isRedirect << 0) | (privateRelayed << 1));
    if (isRedirect)
        encoder << m_redirectRequest;

    encoder << m_maxAgeCap;
    
    encoder.encodeChecksum();

    Data header(encoder.span());
    Data body;
    if (m_buffer) {
        m_buffer = m_buffer->makeContiguous();
        body = { downcast<WebCore::SharedBuffer>(*m_buffer).span() };
    }

    return { m_key, m_timeStamp, header, body, { } };
}

std::unique_ptr<Entry> Entry::decodeStorageRecord(const Storage::Record& storageEntry)
{
    auto entry = makeUnique<Entry>(storageEntry);

    WTF::Persistence::Decoder decoder(storageEntry.header.span());
    std::optional<WebCore::ResourceResponse> response;
    decoder >> response;
    if (!response)
        return nullptr;
    entry->m_response = WTFMove(*response);
    entry->m_response.setSource(WebCore::ResourceResponse::Source::DiskCache);

    std::optional<bool> hasVaryingRequestHeaders;
    decoder >> hasVaryingRequestHeaders;
    if (!hasVaryingRequestHeaders)
        return nullptr;

    if (*hasVaryingRequestHeaders) {
        std::optional<Vector<std::pair<String, String>>> varyingRequestHeaders;
        decoder >> varyingRequestHeaders;
        if (!varyingRequestHeaders)
            return nullptr;
        entry->m_varyingRequestHeaders = WTFMove(*varyingRequestHeaders);
    }

    std::optional<uint8_t> isRedirectAndPrivateRelayed;
    decoder >> isRedirectAndPrivateRelayed;
    if (!isRedirectAndPrivateRelayed)
        return nullptr;

    bool isRedirect = *isRedirectAndPrivateRelayed & 0x1;
    entry->m_privateRelayed = *isRedirectAndPrivateRelayed & 0x2 ? PrivateRelayed::Yes : PrivateRelayed::No;
    
    if (isRedirect) {
        entry->m_redirectRequest.emplace();
        std::optional<std::optional<WebCore::ResourceRequest>> resourceRequest;
        decoder >> resourceRequest;
        if (!resourceRequest)
            return nullptr;
        entry->m_redirectRequest = WTFMove(*resourceRequest);
    }

    std::optional<std::optional<Seconds>> maxAgeCap;
    decoder >> maxAgeCap;
    if (!maxAgeCap)
        return nullptr;
    entry->m_maxAgeCap = WTFMove(*maxAgeCap);

    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        return nullptr;
    }

    return entry;
}

bool Entry::hasReachedPrevalentResourceAgeCap() const
{
    return m_maxAgeCap && WebCore::computeCurrentAge(response(), timeStamp()) > m_maxAgeCap;
}

void Entry::capMaxAge(const Seconds seconds)
{
    m_maxAgeCap = seconds;
}

void Entry::initializeBufferFromStorageRecord() const
{
#if ENABLE(SHAREABLE_RESOURCE)
    if (auto handle = shareableResourceHandle()) {
        m_buffer = WTFMove(*handle).tryWrapInSharedBuffer();
        if (m_buffer)
            return;
    }
#endif
    m_buffer = WebCore::SharedBuffer::create(m_sourceStorageRecord.body.span());
}

WebCore::FragmentedSharedBuffer* Entry::buffer() const
{
    if (!m_buffer)
        initializeBufferFromStorageRecord();

    return m_buffer.get();
}

RefPtr<WebCore::FragmentedSharedBuffer> Entry::protectedBuffer() const
{
    return buffer();
}

#if ENABLE(SHAREABLE_RESOURCE)
std::optional<WebCore::ShareableResource::Handle> Entry::shareableResourceHandle() const
{
    if (m_shareableResource)
        return m_shareableResource->createHandle();

    auto sharedMemory = m_sourceStorageRecord.body.tryCreateSharedMemory();
    if (!sharedMemory)
        return std::nullopt;

    if ((m_shareableResource = WebCore::ShareableResource::create(sharedMemory.releaseNonNull(), 0, m_sourceStorageRecord.body.size())))
        return m_shareableResource->createHandle();
    return std::nullopt;
}
#endif

bool Entry::needsValidation() const
{
    return m_response.source() == WebCore::ResourceResponse::Source::DiskCacheAfterValidation;
}

void Entry::setNeedsValidation(bool value)
{
    m_response.setSource(value ? WebCore::ResourceResponse::Source::DiskCacheAfterValidation : WebCore::ResourceResponse::Source::DiskCache);
}

void Entry::asJSON(StringBuilder& json, const Storage::RecordInfo& info) const
{
    json.append("{\n"_s
        "\"hash\": "_s);
    json.appendQuotedJSONString(m_key.hashAsString());
    json.append(",\n"_s
        "\"bodySize\": "_s, info.bodySize, ",\n"_s
        "\"worth\": "_s, info.worth, ",\n"_s
        "\"partition\": "_s);
    json.appendQuotedJSONString(m_key.partition());
    json.append(",\n"_s
        "\"timestamp\": "_s, m_timeStamp.secondsSinceEpoch().milliseconds(), ",\n"_s
        "\"URL\": "_s);
    json.appendQuotedJSONString(m_response.url().string());
    json.append(",\n"_s
        "\"bodyHash\": "_s);
    json.appendQuotedJSONString(info.bodyHash);
    json.append(",\n"_s
        "\"bodyShareCount\": "_s, info.bodyShareCount, ",\n"_s
        "\"headers\": {\n"_s);
    bool firstHeader = true;
    for (auto& header : m_response.httpHeaderFields()) {
        json.append(std::exchange(firstHeader, false) ? ""_s : ",\n"_s, "    "_s);
        json.appendQuotedJSONString(header.key);
        json.append(": "_s);
        json.appendQuotedJSONString(header.value);
    }
    json.append("\n"_s
        "}\n"_s
        "}"_s);
}

}
}
