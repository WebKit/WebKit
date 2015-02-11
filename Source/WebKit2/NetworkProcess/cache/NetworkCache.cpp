/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CACHE)

#include "Logging.h"
#include "NetworkCacheCoders.h"
#include "NetworkCacheStatistics.h"
#include "NetworkCacheStorage.h"
#include "NetworkResourceLoader.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/CacheValidation.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringHasher.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

NetworkCache& NetworkCache::singleton()
{
    static NeverDestroyed<NetworkCache> instance;
    return instance;
}

bool NetworkCache::initialize(const String& cachePath, bool enableEfficacyLogging)
{
    m_storage = NetworkCacheStorage::open(cachePath);

    if (enableEfficacyLogging)
        m_statistics = NetworkCacheStatistics::open(cachePath);

    LOG(NetworkCache, "(NetworkProcess) opened cache storage, success %d", !!m_storage);
    return !!m_storage;
}

void NetworkCache::setMaximumSize(size_t maximumSize)
{
    if (!m_storage)
        return;
    m_storage->setMaximumSize(maximumSize);
}

static NetworkCacheStorage::Entry encodeStorageEntry(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, PassRefPtr<WebCore::SharedBuffer> responseData)
{
    NetworkCacheEncoder encoder;
    encoder << response;

    String varyValue = response.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    bool hasVaryingRequestHeaders = !varyValue.isEmpty();

    encoder << hasVaryingRequestHeaders;

    if (hasVaryingRequestHeaders) {
        Vector<String> varyingHeaderNames;
        varyValue.split(',', false, varyingHeaderNames);

        Vector<std::pair<String, String>> varyingRequestHeaders;
        for (auto& varyHeaderName : varyingHeaderNames) {
            String headerName = varyHeaderName.stripWhiteSpace();
            varyingRequestHeaders.append(std::make_pair(headerName, request.httpHeaderField(headerName)));
        }
        encoder << varyingRequestHeaders;
    }
    encoder.encodeChecksum();

    auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    NetworkCacheStorage::Data header(encoder.buffer(), encoder.bufferSize());
    NetworkCacheStorage::Data body;
    if (responseData)
        body = NetworkCacheStorage::Data(reinterpret_cast<const uint8_t*>(responseData->data()), responseData->size());

    return NetworkCacheStorage::Entry { timeStamp, header, body };
}

static bool verifyVaryingRequestHeaders(const Vector<std::pair<String, String>>& varyingRequestHeaders, const WebCore::ResourceRequest& request)
{
    for (auto& varyingRequestHeader : varyingRequestHeaders) {
        // FIXME: Vary: * in response would ideally trigger a cache delete instead of a store.
        if (varyingRequestHeader.first == "*")
            return false;
        String requestValue = request.httpHeaderField(varyingRequestHeader.first);
        if (requestValue != varyingRequestHeader.second)
            return false;
    }
    return true;
}

static bool cachePolicyAllowsExpired(WebCore::ResourceRequestCachePolicy policy)
{
    switch (policy) {
    case WebCore::ReturnCacheDataElseLoad:
    case WebCore::ReturnCacheDataDontLoad:
        return true;
    case WebCore::UseProtocolCachePolicy:
    case WebCore::ReloadIgnoringCacheData:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static std::unique_ptr<NetworkCache::Entry> decodeStorageEntry(const NetworkCacheStorage::Entry& storageEntry, const WebCore::ResourceRequest& request)
{
    NetworkCacheDecoder decoder(storageEntry.header.data(), storageEntry.header.size());

    WebCore::ResourceResponse cachedResponse;
    if (!decoder.decode(cachedResponse)) {
        LOG(NetworkCache, "(NetworkProcess) response decoding failed\n");
        return nullptr;
    }

    bool hasVaryingRequestHeaders;
    if (!decoder.decode(hasVaryingRequestHeaders))
        return nullptr;

    if (hasVaryingRequestHeaders) {
        Vector<std::pair<String, String>> varyingRequestHeaders;
        if (!decoder.decode(varyingRequestHeaders))
            return nullptr;

        if (!verifyVaryingRequestHeaders(varyingRequestHeaders, request)) {
            LOG(NetworkCache, "(NetworkProcess) varying header mismatch\n");
            return nullptr;
        }
    }
    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        return nullptr;
    }

    bool allowExpired = cachePolicyAllowsExpired(request.cachePolicy()) && !cachedResponse.cacheControlContainsMustRevalidate();
    auto timeStamp = std::chrono::duration_cast<std::chrono::duration<double>>(storageEntry.timeStamp);
    double age = WebCore::computeCurrentAge(cachedResponse, timeStamp.count());
    double lifetime = WebCore::computeFreshnessLifetimeForHTTPFamily(cachedResponse, timeStamp.count());
    bool isExpired = age > lifetime;
    bool needsRevalidation = (isExpired && !allowExpired) || cachedResponse.cacheControlContainsNoCache();

    if (needsRevalidation) {
        bool hasValidatorFields = cachedResponse.hasCacheValidatorFields();
        LOG(NetworkCache, "(NetworkProcess) needsRevalidation hasValidatorFields=%d isExpired=%d age=%f lifetime=%f", isExpired, hasValidatorFields, age, lifetime);
        if (!hasValidatorFields)
            return nullptr;
    }

    auto entry = std::make_unique<NetworkCache::Entry>();
    entry->storageEntry = storageEntry;
    entry->needsRevalidation = needsRevalidation;

    cachedResponse.setSource(needsRevalidation ? WebCore::ResourceResponse::Source::DiskCacheAfterValidation : WebCore::ResourceResponse::Source::DiskCache);
    entry->response = cachedResponse;

#if ENABLE(SHAREABLE_RESOURCE)
    RefPtr<SharedMemory> sharedMemory = storageEntry.body.isMap() ? SharedMemory::createFromVMBuffer(const_cast<uint8_t*>(storageEntry.body.data()), storageEntry.body.size()) : nullptr;
    RefPtr<ShareableResource> shareableResource = sharedMemory ? ShareableResource::create(sharedMemory.release(), 0, storageEntry.body.size()) : nullptr;

    if (shareableResource && shareableResource->createHandle(entry->shareableResourceHandle))
        entry->buffer = entry->shareableResourceHandle.tryWrapInSharedBuffer();
    else
#endif
        entry->buffer = WebCore::SharedBuffer::create(storageEntry.body.data(), storageEntry.body.size());

    return entry;
}

static bool canRetrieve(const WebCore::ResourceRequest& request)
{
    if (!request.url().protocolIsInHTTPFamily())
        return false;
    // FIXME: Support HEAD and OPTIONS requests.
    if (request.httpMethod() != "GET")
        return false;
    // FIXME: We should be able to validate conditional requests using cache.
    if (request.isConditional())
        return false;
    if (request.cachePolicy() == WebCore::ReloadIgnoringCacheData)
        return false;

    return true;
}

static NetworkCacheKey makeCacheKey(const WebCore::ResourceRequest& request)
{
#if ENABLE(CACHE_PARTITIONING)
    String partition = request.cachePartition();
#else
    String partition;
#endif
    if (partition.isEmpty())
        partition = ASCIILiteral("No partition");
    return NetworkCacheKey(request.httpMethod(), partition, request.url().string());
}

void NetworkCache::retrieve(const WebCore::ResourceRequest& originalRequest, std::function<void (std::unique_ptr<Entry>)> completionHandler)
{
    ASSERT(isEnabled());

    LOG(NetworkCache, "(NetworkProcess) retrieving %s priority %u", originalRequest.url().string().ascii().data(), originalRequest.priority());

    NetworkCacheKey storageKey = makeCacheKey(originalRequest);
    if (!canRetrieve(originalRequest)) {
        if (m_statistics)
            m_statistics->recordNotUsingCacheForRequest(storageKey, originalRequest);

        completionHandler(nullptr);
        return;
    }

    auto startTime = std::chrono::system_clock::now();
    unsigned priority = originalRequest.priority();

    m_storage->retrieve(storageKey, priority, [this, originalRequest, completionHandler, startTime, storageKey](std::unique_ptr<NetworkCacheStorage::Entry> entry) {
        if (!entry) {
            LOG(NetworkCache, "(NetworkProcess) not found in storage");

            if (m_statistics)
                m_statistics->recordRetrievalFailure(storageKey, originalRequest);

            completionHandler(nullptr);
            return false;
        }
        auto decodedEntry = decodeStorageEntry(*entry, originalRequest);
        bool success = !!decodedEntry;
        if (m_statistics)
            m_statistics->recordRetrievedCachedEntry(storageKey, originalRequest, success);

#if !LOG_DISABLED
        auto elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
#endif
        LOG(NetworkCache, "(NetworkProcess) retrieve complete success=%d priority=%u time=%lldms", success, originalRequest.priority(), elapsedMS);
        completionHandler(WTF::move(decodedEntry));
        return success;
    });
}

static bool canStore(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response)
{
    if (!originalRequest.url().protocolIsInHTTPFamily()) {
        LOG(NetworkCache, "(NetworkProcess) not HTTP");
        return false;
    }
    if (originalRequest.httpMethod() != "GET") {
        LOG(NetworkCache, "(NetworkProcess) method %s", originalRequest.httpMethod().utf8().data());
        return false;
    }
    if (response.isAttachment()) {
        LOG(NetworkCache, "(NetworkProcess) attachment");
        return false;
    }

    switch (response.httpStatusCode()) {
    case 200: // OK
    case 203: // Non-Authoritative Information
    case 300: // Multiple Choices
    case 301: // Moved Permanently
    case 302: // Found
    case 307: // Temporary Redirect
    case 410: // Gone
        if (response.cacheControlContainsNoStore()) {
            LOG(NetworkCache, "(NetworkProcess) Cache-control:no-store");
            return false;
        }
        return true;
    default:
        LOG(NetworkCache, "(NetworkProcess) status code %d", response.httpStatusCode());
    }

    return false;
}

void NetworkCache::store(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response, RefPtr<WebCore::SharedBuffer>&& responseData, std::function<void (MappedBody&)> completionHandler)
{
    ASSERT(isEnabled());
    ASSERT(responseData);

    LOG(NetworkCache, "(NetworkProcess) storing %s, partition %s", originalRequest.url().string().latin1().data(), originalRequest.cachePartition().latin1().data());

    if (!canStore(originalRequest, response)) {
        LOG(NetworkCache, "(NetworkProcess) didn't store");
        return;
    }

    auto key = makeCacheKey(originalRequest);
    auto storageEntry = encodeStorageEntry(originalRequest, response, WTF::move(responseData));

    m_storage->store(key, storageEntry, [completionHandler](bool success, const NetworkCacheStorage::Data& bodyData) {
        MappedBody mappedBody;
#if ENABLE(SHAREABLE_RESOURCE)
        if (bodyData.isMap()) {
            RefPtr<SharedMemory> sharedMemory = SharedMemory::createFromVMBuffer(const_cast<uint8_t*>(bodyData.data()), bodyData.size());
            mappedBody.shareableResource = sharedMemory ? ShareableResource::create(WTF::move(sharedMemory), 0, bodyData.size()) : nullptr;
            if (mappedBody.shareableResource)
                mappedBody.shareableResource->createHandle(mappedBody.shareableResourceHandle);
        }
#endif
        completionHandler(mappedBody);
        LOG(NetworkCache, "(NetworkProcess) store success=%d", success);
    });
}

void NetworkCache::update(const WebCore::ResourceRequest& originalRequest, const Entry& entry, const WebCore::ResourceResponse& validatingResponse)
{
    LOG(NetworkCache, "(NetworkProcess) updating %s", originalRequest.url().string().latin1().data());

    WebCore::ResourceResponse response = entry.response;
    WebCore::updateResponseHeadersAfterRevalidation(response, validatingResponse);

    auto key = makeCacheKey(originalRequest);
    auto updateEntry = encodeStorageEntry(originalRequest, response, entry.buffer);

    m_storage->update(key, updateEntry, entry.storageEntry, [](bool success, const NetworkCacheStorage::Data&) {
        LOG(NetworkCache, "(NetworkProcess) updated, success=%d", success);
    });
}

void NetworkCache::clear()
{
    LOG(NetworkCache, "(NetworkProcess) clearing cache");
    if (m_storage)
        m_storage->clear();
    if (m_statistics)
        m_statistics->clear();
}

String NetworkCache::storagePath() const
{
    return m_storage ? m_storage->directoryPath() : String();
}

}

#endif
