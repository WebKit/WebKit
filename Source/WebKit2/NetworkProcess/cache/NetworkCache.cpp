/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/JSONObject.h>
#include <WebCore/CacheValidation.h>
#include <WebCore/FileSystem.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StringHasher.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <notify.h>
#endif

namespace WebKit {
namespace NetworkCache {

Cache& singleton()
{
    static NeverDestroyed<Cache> instance;
    return instance;
}

bool Cache::initialize(const String& cachePath, bool enableEfficacyLogging)
{
    m_storage = Storage::open(cachePath);

    if (enableEfficacyLogging)
        m_statistics = Statistics::open(cachePath);

#if PLATFORM(COCOA)
    // Triggers with "notifyutil -p com.apple.WebKit.Cache.dump".
    if (m_storage) {
        int token;
        notify_register_dispatch("com.apple.WebKit.Cache.dump", &token, dispatch_get_main_queue(), ^(int) {
            dumpContentsToFile();
        });
    }
#endif

    LOG(NetworkCache, "(NetworkProcess) opened cache storage, success %d", !!m_storage);
    return !!m_storage;
}

void Cache::setMaximumSize(size_t maximumSize)
{
    if (!m_storage)
        return;
    m_storage->setMaximumSize(maximumSize);
}

static Key makeCacheKey(const WebCore::ResourceRequest& request)
{
#if ENABLE(CACHE_PARTITIONING)
    String partition = request.cachePartition();
#else
    String partition;
#endif
    if (partition.isEmpty())
        partition = ASCIILiteral("No partition");
    return { request.httpMethod(), partition, request.url().string()  };
}

static String headerValueForVary(const WebCore::ResourceRequest& request, const String& headerName)
{
    // Explicit handling for cookies is needed because they are added magically by the networking layer.
    // FIXME: The value might have changed between making the request and retrieving the cookie here.
    // We could fetch the cookie when making the request but that seems overkill as the case is very rare and it
    // is a blocking operation. This should be sufficient to cover reasonable cases.
    if (headerName == httpHeaderNameString(WebCore::HTTPHeaderName::Cookie))
        return WebCore::cookieRequestHeaderFieldValue(WebCore::NetworkStorageSession::defaultStorageSession(), request.firstPartyForCookies(), request.url());
    return request.httpHeaderField(headerName);
}

static Storage::Entry encodeStorageEntry(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response, PassRefPtr<WebCore::SharedBuffer> responseData)
{
    Encoder encoder;
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
            String headerValue = headerValueForVary(request, headerName);
            varyingRequestHeaders.append(std::make_pair(headerName, headerValue));
        }
        encoder << varyingRequestHeaders;
    }
    encoder.encodeChecksum();

    auto timeStamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    Data header(encoder.buffer(), encoder.bufferSize());
    Data body;
    if (responseData)
        body = { reinterpret_cast<const uint8_t*>(responseData->data()), responseData->size() };

    return { makeCacheKey(request), timeStamp, header, body };
}

static bool verifyVaryingRequestHeaders(const Vector<std::pair<String, String>>& varyingRequestHeaders, const WebCore::ResourceRequest& request)
{
    for (auto& varyingRequestHeader : varyingRequestHeaders) {
        // FIXME: Vary: * in response would ideally trigger a cache delete instead of a store.
        if (varyingRequestHeader.first == "*")
            return false;
        String headerValue = headerValueForVary(request, varyingRequestHeader.first);
        if (headerValue != varyingRequestHeader.second)
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

static std::unique_ptr<Entry> decodeStorageEntry(const Storage::Entry& storageEntry, const WebCore::ResourceRequest& request, CachedEntryReuseFailure& failure)
{
    Decoder decoder(storageEntry.header.data(), storageEntry.header.size());

    WebCore::ResourceResponse cachedResponse;
    if (!decoder.decode(cachedResponse)) {
        LOG(NetworkCache, "(NetworkProcess) response decoding failed\n");
        failure = CachedEntryReuseFailure::Other;
        return nullptr;
    }

    bool hasVaryingRequestHeaders;
    if (!decoder.decode(hasVaryingRequestHeaders)) {
        failure = CachedEntryReuseFailure::Other;
        return nullptr;
    }

    if (hasVaryingRequestHeaders) {
        Vector<std::pair<String, String>> varyingRequestHeaders;
        if (!decoder.decode(varyingRequestHeaders)) {
            failure = CachedEntryReuseFailure::Other;
            return nullptr;
        }

        if (!verifyVaryingRequestHeaders(varyingRequestHeaders, request)) {
            LOG(NetworkCache, "(NetworkProcess) varying header mismatch\n");
            failure = CachedEntryReuseFailure::VaryingHeaderMismatch;
            return nullptr;
        }
    }
    if (!decoder.verifyChecksum()) {
        LOG(NetworkCache, "(NetworkProcess) checksum verification failure\n");
        failure = CachedEntryReuseFailure::Other;
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
        if (!hasValidatorFields) {
            failure = CachedEntryReuseFailure::MissingValidatorFields;
            return nullptr;
        }
    }

    auto entry = std::make_unique<Entry>();
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

static RetrieveDecision canRetrieve(const WebCore::ResourceRequest& request)
{
    // FIXME: Support HEAD and OPTIONS requests.
    if (request.httpMethod() != "GET")
        return RetrieveDecision::NoDueToHTTPMethod;
    // FIXME: We should be able to validate conditional requests using cache.
    if (request.isConditional())
        return RetrieveDecision::NoDueToConditionalRequest;
    if (request.cachePolicy() == WebCore::ReloadIgnoringCacheData)
        return RetrieveDecision::NoDueToReloadIgnoringCache;

    return RetrieveDecision::Yes;
}

void Cache::retrieve(const WebCore::ResourceRequest& originalRequest, uint64_t webPageID, std::function<void (std::unique_ptr<Entry>)> completionHandler)
{
    ASSERT(isEnabled());
    ASSERT(originalRequest.url().protocolIsInHTTPFamily());

    LOG(NetworkCache, "(NetworkProcess) retrieving %s priority %u", originalRequest.url().string().ascii().data(), originalRequest.priority());

    Key storageKey = makeCacheKey(originalRequest);
    RetrieveDecision retrieveDecision = canRetrieve(originalRequest);
    if (retrieveDecision != RetrieveDecision::Yes) {
        if (m_statistics)
            m_statistics->recordNotUsingCacheForRequest(webPageID, storageKey, originalRequest, retrieveDecision);

        completionHandler(nullptr);
        return;
    }

    auto startTime = std::chrono::system_clock::now();
    unsigned priority = originalRequest.priority();

    m_storage->retrieve(storageKey, priority, [this, originalRequest, completionHandler, startTime, storageKey, webPageID](std::unique_ptr<Storage::Entry> entry) {
        if (!entry) {
            LOG(NetworkCache, "(NetworkProcess) not found in storage");

            if (m_statistics)
                m_statistics->recordRetrievalFailure(webPageID, storageKey, originalRequest);

            completionHandler(nullptr);
            return false;
        }
        ASSERT(entry->key == storageKey);

        CachedEntryReuseFailure failure = CachedEntryReuseFailure::None;
        auto decodedEntry = decodeStorageEntry(*entry, originalRequest, failure);
        bool success = !!decodedEntry;
        if (m_statistics)
            m_statistics->recordRetrievedCachedEntry(webPageID, storageKey, originalRequest, failure);

#if !LOG_DISABLED
        auto elapsedMS = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count();
#endif
        LOG(NetworkCache, "(NetworkProcess) retrieve complete success=%d priority=%u time=%lldms", success, originalRequest.priority(), elapsedMS);
        completionHandler(WTF::move(decodedEntry));
        return success;
    });
}

static StoreDecision canStore(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response)
{
    if (!originalRequest.url().protocolIsInHTTPFamily() || !response.isHTTP()) {
        LOG(NetworkCache, "(NetworkProcess) not HTTP");
        return StoreDecision::NoDueToProtocol;
    }
    if (originalRequest.httpMethod() != "GET") {
        LOG(NetworkCache, "(NetworkProcess) method %s", originalRequest.httpMethod().utf8().data());
        return StoreDecision::NoDueToHTTPMethod;
    }
    if (response.isAttachment()) {
        LOG(NetworkCache, "(NetworkProcess) attachment");
        return StoreDecision::NoDueToAttachmentResponse;
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
            return StoreDecision::NoDueToNoStoreResponse;
        }
        return StoreDecision::Yes;
    default:
        LOG(NetworkCache, "(NetworkProcess) status code %d", response.httpStatusCode());
    }

    return StoreDecision::NoDueToHTTPStatusCode;
}

void Cache::store(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response, RefPtr<WebCore::SharedBuffer>&& responseData, std::function<void (MappedBody&)> completionHandler)
{
    ASSERT(isEnabled());
    ASSERT(responseData);

    LOG(NetworkCache, "(NetworkProcess) storing %s, partition %s", originalRequest.url().string().latin1().data(), originalRequest.cachePartition().latin1().data());

    StoreDecision storeDecision = canStore(originalRequest, response);
    if (storeDecision != StoreDecision::Yes) {
        LOG(NetworkCache, "(NetworkProcess) didn't store");
        if (m_statistics) {
            auto key = makeCacheKey(originalRequest);
            m_statistics->recordNotCachingResponse(key, storeDecision);
        }
        return;
    }

    auto storageEntry = encodeStorageEntry(originalRequest, response, WTF::move(responseData));

    m_storage->store(storageEntry, [completionHandler](bool success, const Data& bodyData) {
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

void Cache::update(const WebCore::ResourceRequest& originalRequest, const Entry& entry, const WebCore::ResourceResponse& validatingResponse)
{
    LOG(NetworkCache, "(NetworkProcess) updating %s", originalRequest.url().string().latin1().data());

    WebCore::ResourceResponse response = entry.response;
    WebCore::updateResponseHeadersAfterRevalidation(response, validatingResponse);

    auto updateEntry = encodeStorageEntry(originalRequest, response, entry.buffer);

    m_storage->update(updateEntry, entry.storageEntry, [](bool success, const Data&) {
        LOG(NetworkCache, "(NetworkProcess) updated, success=%d", success);
    });
}

void Cache::remove(const Entry& entry)
{
    ASSERT(isEnabled());

    m_storage->remove(entry.storageEntry.key);
}

void Cache::traverse(std::function<void (const Entry*)>&& traverseHandler)
{
    ASSERT(isEnabled());

    m_storage->traverse([traverseHandler](const Storage::Entry* entry) {
        if (!entry) {
            traverseHandler(nullptr);
            return;
        }

        Entry cacheEntry;
        cacheEntry.storageEntry = *entry;

        Decoder decoder(cacheEntry.storageEntry.header.data(), cacheEntry.storageEntry.header.size());
        if (!decoder.decode(cacheEntry.response))
            return;

        traverseHandler(&cacheEntry);
    });
}

String Cache::dumpFilePath() const
{
    return WebCore::pathByAppendingComponent(m_storage->baseDirectoryPath(), "dump.json");
}

static bool entryAsJSON(StringBuilder& json, const Storage::Entry& entry)
{
    Decoder decoder(entry.header.data(), entry.header.size());
    WebCore::ResourceResponse cachedResponse;
    if (!decoder.decode(cachedResponse))
        return false;
    json.append("{\n");
    json.append("\"hash\": ");
    JSC::appendQuotedJSONStringToBuilder(json, entry.key.hashAsString());
    json.append(",\n");
    json.append("\"partition\": ");
    JSC::appendQuotedJSONStringToBuilder(json, entry.key.partition());
    json.append(",\n");
    json.append("\"timestamp\": ");
    json.appendNumber(entry.timeStamp.count());
    json.append(",\n");
    json.append("\"URL\": ");
    JSC::appendQuotedJSONStringToBuilder(json, cachedResponse.url().string());
    json.append(",\n");
    json.append("\"headers\": {\n");
    bool firstHeader = true;
    for (auto& header : cachedResponse.httpHeaderFields()) {
        if (!firstHeader)
            json.append(",\n");
        firstHeader = false;
        json.append("    ");
        JSC::appendQuotedJSONStringToBuilder(json, header.key);
        json.append(": ");
        JSC::appendQuotedJSONStringToBuilder(json, header.value);
    }
    json.append("\n}\n");
    json.append("}");
    return true;
}

void Cache::dumpContentsToFile()
{
    if (!m_storage)
        return;
    auto dumpFileHandle = WebCore::openFile(dumpFilePath(), WebCore::OpenForWrite);
    if (!dumpFileHandle)
        return;
    WebCore::writeToFile(dumpFileHandle, "[\n", 2);
    m_storage->traverse([dumpFileHandle](const Storage::Entry* entry) {
        if (!entry) {
            WebCore::writeToFile(dumpFileHandle, "{}\n]\n", 5);
            auto handle = dumpFileHandle;
            WebCore::closeFile(handle);
            return;
        }
        StringBuilder json;
        if (!entryAsJSON(json, *entry))
            return;
        json.append(",\n");
        auto writeData = json.toString().utf8();
        WebCore::writeToFile(dumpFileHandle, writeData.data(), writeData.length());
    });
}

void Cache::clear()
{
    LOG(NetworkCache, "(NetworkProcess) clearing cache");
    if (m_storage) {
        m_storage->clear();

        auto queue = WorkQueue::create("com.apple.WebKit.Cache.delete");
        StringCapture dumpFilePathCapture(dumpFilePath());
        queue->dispatch([dumpFilePathCapture] {
            WebCore::deleteFile(dumpFilePathCapture.string());
        });
    }
    if (m_statistics)
        m_statistics->clear();
}

String Cache::storagePath() const
{
    return m_storage ? m_storage->directoryPath() : String();
}

}
}

#endif
