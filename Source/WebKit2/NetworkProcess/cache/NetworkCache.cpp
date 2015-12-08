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
#include "NetworkCacheSpeculativeLoadManager.h"
#include "NetworkCacheStatistics.h"
#include "NetworkCacheStorage.h"
#include <WebCore/CacheValidation.h>
#include <WebCore/FileSystem.h>
#include <WebCore/HTTPHeaderNames.h>
#include <WebCore/NetworkStorageSession.h>
#include <WebCore/PlatformCookieJar.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(COCOA)
#include <notify.h>
#endif

namespace WebKit {
namespace NetworkCache {

static const AtomicString& resourceType()
{
    ASSERT(WTF::isMainThread());
    static NeverDestroyed<const AtomicString> resource("resource", AtomicString::ConstructFromLiteral);
    return resource;
}

Cache& singleton()
{
    static NeverDestroyed<Cache> instance;
    return instance;
}

#if PLATFORM(GTK)
static void dumpFileChanged(Cache* cache)
{
    cache->dumpContentsToFile();
}
#endif

bool Cache::initialize(const String& cachePath, const Parameters& parameters)
{
    m_storage = Storage::open(cachePath);

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (parameters.enableNetworkCacheSpeculativeRevalidation)
        m_speculativeLoadManager = std::make_unique<SpeculativeLoadManager>(*m_storage);
#endif

    if (parameters.enableEfficacyLogging)
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
#if PLATFORM(GTK)
    // Triggers with "touch $cachePath/dump".
    if (m_storage) {
        CString dumpFilePath = WebCore::fileSystemRepresentation(WebCore::pathByAppendingComponent(m_storage->basePath(), "dump"));
        GRefPtr<GFile> dumpFile = adoptGRef(g_file_new_for_path(dumpFilePath.data()));
        GFileMonitor* monitor = g_file_monitor_file(dumpFile.get(), G_FILE_MONITOR_NONE, nullptr, nullptr);
        g_signal_connect_swapped(monitor, "changed", G_CALLBACK(dumpFileChanged), this);
    }
#endif

    LOG(NetworkCache, "(NetworkProcess) opened cache storage, success %d", !!m_storage);
    return !!m_storage;
}

void Cache::setCapacity(size_t maximumSize)
{
    if (!m_storage)
        return;
    m_storage->setCapacity(maximumSize);
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

    // FIXME: This implements minimal Range header disk cache support. We don't parse
    // ranges so only the same exact range request will be served from the cache.
    String range = request.httpHeaderField(WebCore::HTTPHeaderName::Range);
    return { partition, resourceType(), range, request.url().string() };
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

static Vector<std::pair<String, String>> collectVaryingRequestHeaders(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
{
    String varyValue = response.httpHeaderField(WebCore::HTTPHeaderName::Vary);
    if (varyValue.isEmpty())
        return { };
    Vector<String> varyingHeaderNames;
    varyValue.split(',', /*allowEmptyEntries*/ false, varyingHeaderNames);
    Vector<std::pair<String, String>> varyingRequestHeaders;
    varyingRequestHeaders.reserveCapacity(varyingHeaderNames.size());
    for (auto& varyHeaderName : varyingHeaderNames) {
        String headerName = varyHeaderName.stripWhiteSpace();
        String headerValue = headerValueForVary(request, headerName);
        varyingRequestHeaders.append(std::make_pair(headerName, headerValue));
    }
    return varyingRequestHeaders;
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

static bool responseHasExpired(const WebCore::ResourceResponse& response, std::chrono::system_clock::time_point timestamp, Optional<std::chrono::microseconds> maxStale)
{
    if (response.cacheControlContainsNoCache())
        return true;

    auto age = WebCore::computeCurrentAge(response, timestamp);
    auto lifetime = WebCore::computeFreshnessLifetimeForHTTPFamily(response, timestamp);

    auto maximumStaleness = maxStale ? maxStale.value() : 0_ms;
    bool hasExpired = age - lifetime > maximumStaleness;

#ifndef LOG_DISABLED
    if (hasExpired)
        LOG(NetworkCache, "(NetworkProcess) needsRevalidation hasExpired age=%f lifetime=%f max-stale=%g", age, lifetime, maxStale);
#endif

    return hasExpired;
}

static bool responseNeedsRevalidation(const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, std::chrono::system_clock::time_point timestamp)
{
    auto requestDirectives = WebCore::parseCacheControlDirectives(request.httpHeaderFields());
    if (requestDirectives.noCache)
        return true;
    // For requests we ignore max-age values other than zero.
    if (requestDirectives.maxAge && requestDirectives.maxAge.value() == 0_ms)
        return true;

    return responseHasExpired(response, timestamp, requestDirectives.maxStale);
}

static UseDecision makeUseDecision(const Entry& entry, const WebCore::ResourceRequest& request)
{
    // The request is conditional so we force revalidation from the network. We merely check the disk cache
    // so we can update the cache entry.
    if (request.isConditional())
        return UseDecision::Validate;

    if (!verifyVaryingRequestHeaders(entry.varyingRequestHeaders(), request))
        return UseDecision::NoDueToVaryingHeaderMismatch;

    // We never revalidate in the case of a history navigation.
    if (cachePolicyAllowsExpired(request.cachePolicy()))
        return UseDecision::Use;

    if (!responseNeedsRevalidation(entry.response(), request, entry.timeStamp()))
        return UseDecision::Use;

    if (!entry.response().hasCacheValidatorFields())
        return UseDecision::NoDueToMissingValidatorFields;

    return UseDecision::Validate;
}

static RetrieveDecision makeRetrieveDecision(const WebCore::ResourceRequest& request)
{
    // FIXME: Support HEAD requests.
    if (request.httpMethod() != "GET")
        return RetrieveDecision::NoDueToHTTPMethod;
    if (request.cachePolicy() == WebCore::ReloadIgnoringCacheData && !request.isConditional())
        return RetrieveDecision::NoDueToReloadIgnoringCache;

    return RetrieveDecision::Yes;
}

// http://tools.ietf.org/html/rfc7231#page-48
static bool isStatusCodeCacheableByDefault(int statusCode)
{
    switch (statusCode) {
    case 200: // OK
    case 203: // Non-Authoritative Information
    case 204: // No Content
    case 206: // Partial Content
    case 300: // Multiple Choices
    case 301: // Moved Permanently
    case 404: // Not Found
    case 405: // Method Not Allowed
    case 410: // Gone
    case 414: // URI Too Long
    case 501: // Not Implemented
        return true;
    default:
        return false;
    }
}

static bool isStatusCodePotentiallyCacheable(int statusCode)
{
    switch (statusCode) {
    case 201: // Created
    case 202: // Accepted
    case 205: // Reset Content
    case 302: // Found
    case 303: // See Other
    case 307: // Temporary redirect
    case 403: // Forbidden
    case 406: // Not Acceptable
    case 415: // Unsupported Media Type
        return true;
    default:
        return false;
    }
}

static bool isMediaMIMEType(const String& mimeType)
{
    if (mimeType.startsWith("video/", /*caseSensitive*/ false))
        return true;
    if (mimeType.startsWith("audio/", /*caseSensitive*/ false))
        return true;
    return false;
}

static StoreDecision makeStoreDecision(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response)
{
    if (!originalRequest.url().protocolIsInHTTPFamily() || !response.isHTTP())
        return StoreDecision::NoDueToProtocol;

    if (originalRequest.httpMethod() != "GET")
        return StoreDecision::NoDueToHTTPMethod;

    auto requestDirectives = WebCore::parseCacheControlDirectives(originalRequest.httpHeaderFields());
    if (requestDirectives.noStore)
        return StoreDecision::NoDueToNoStoreRequest;

    if (response.cacheControlContainsNoStore())
        return StoreDecision::NoDueToNoStoreResponse;

    if (!isStatusCodeCacheableByDefault(response.httpStatusCode())) {
        // http://tools.ietf.org/html/rfc7234#section-4.3.2
        bool hasExpirationHeaders = response.expires() || response.cacheControlMaxAge();
        bool expirationHeadersAllowCaching = isStatusCodePotentiallyCacheable(response.httpStatusCode()) && hasExpirationHeaders;
        if (!expirationHeadersAllowCaching)
            return StoreDecision::NoDueToHTTPStatusCode;
    }

    bool isMainResource = originalRequest.requester() == WebCore::ResourceRequest::Requester::Main;
    bool storeUnconditionallyForHistoryNavigation = isMainResource || originalRequest.priority() == WebCore::ResourceLoadPriority::VeryHigh;
    if (!storeUnconditionallyForHistoryNavigation) {
        auto now = std::chrono::system_clock::now();
        bool hasNonZeroLifetime = !response.cacheControlContainsNoCache() && WebCore::computeFreshnessLifetimeForHTTPFamily(response, now) > 0_ms;

        bool possiblyReusable = response.hasCacheValidatorFields() || hasNonZeroLifetime;
        if (!possiblyReusable)
            return StoreDecision::NoDueToUnlikelyToReuse;
    }

    // Media loaded via XHR is likely being used for MSE streaming (YouTube and Netflix for example).
    // Streaming media fills the cache quickly and is unlikely to be reused.
    // FIXME: We should introduce a separate media cache partition that doesn't affect other resources.
    // FIXME: We should also make sure make the MSE paths are copy-free so we can use mapped buffers from disk effectively.
    bool isLikelyStreamingMedia = originalRequest.requester() == WebCore::ResourceRequest::Requester::XHR && isMediaMIMEType(response.mimeType());
    if (isLikelyStreamingMedia)
        return StoreDecision::NoDueToStreamingMedia;

    return StoreDecision::Yes;
}

void Cache::retrieve(const WebCore::ResourceRequest& originalRequest, const GlobalFrameID& frameID, std::function<void (std::unique_ptr<Entry>)> completionHandler)
{
    ASSERT(isEnabled());
    ASSERT(originalRequest.url().protocolIsInHTTPFamily());

    LOG(NetworkCache, "(NetworkProcess) retrieving %s priority %d", originalRequest.url().string().ascii().data(), static_cast<int>(originalRequest.priority()));

    if (m_statistics)
        m_statistics->recordRetrievalRequest(frameID.first);

    Key storageKey = makeCacheKey(originalRequest);

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (m_speculativeLoadManager)
        m_speculativeLoadManager->registerLoad(frameID, originalRequest, storageKey);
#endif

    auto retrieveDecision = makeRetrieveDecision(originalRequest);
    if (retrieveDecision != RetrieveDecision::Yes) {
        if (m_statistics)
            m_statistics->recordNotUsingCacheForRequest(frameID.first, storageKey, originalRequest, retrieveDecision);

        completionHandler(nullptr);
        return;
    }

#if ENABLE(NETWORK_CACHE_SPECULATIVE_REVALIDATION)
    if (m_speculativeLoadManager && m_speculativeLoadManager->retrieve(frameID, storageKey, [originalRequest, completionHandler](std::unique_ptr<Entry> entry) {
        if (entry && verifyVaryingRequestHeaders(entry->varyingRequestHeaders(), originalRequest))
            completionHandler(WTF::move(entry));
        else
            completionHandler(nullptr);
    }))
        return;
#endif

    auto startTime = std::chrono::system_clock::now();
    auto priority = static_cast<unsigned>(originalRequest.priority());

    m_storage->retrieve(storageKey, priority, [this, originalRequest, completionHandler, startTime, storageKey, frameID](std::unique_ptr<Storage::Record> record) {
        if (!record) {
            LOG(NetworkCache, "(NetworkProcess) not found in storage");

            if (m_statistics)
                m_statistics->recordRetrievalFailure(frameID.first, storageKey, originalRequest);

            completionHandler(nullptr);
            return false;
        }

        ASSERT(record->key == storageKey);

        auto entry = Entry::decodeStorageRecord(*record);

        auto useDecision = entry ? makeUseDecision(*entry, originalRequest) : UseDecision::NoDueToDecodeFailure;
        switch (useDecision) {
        case UseDecision::Use:
            break;
        case UseDecision::Validate:
            entry->setNeedsValidation();
            break;
        default:
            entry = nullptr;
        };

#if !LOG_DISABLED
        auto elapsedMS = static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - startTime).count());
        LOG(NetworkCache, "(NetworkProcess) retrieve complete useDecision=%d priority=%d time=%" PRIi64 "ms", static_cast<int>(useDecision), static_cast<int>(originalRequest.priority()), elapsedMS);
#endif
        completionHandler(WTF::move(entry));

        if (m_statistics)
            m_statistics->recordRetrievedCachedEntry(frameID.first, storageKey, originalRequest, useDecision);
        return useDecision != UseDecision::NoDueToDecodeFailure;
    });
}

std::unique_ptr<Entry> Cache::store(const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceResponse& response, RefPtr<WebCore::SharedBuffer>&& responseData, std::function<void (MappedBody&)> completionHandler)
{
    ASSERT(isEnabled());
    ASSERT(responseData);

#if !LOG_DISABLED
#if ENABLE(CACHE_PARTITIONING)
    CString partition = originalRequest.cachePartition().latin1();
#else
    CString partition = "No partition";
#endif
    LOG(NetworkCache, "(NetworkProcess) storing %s, partition %s", originalRequest.url().string().latin1().data(), partition.data());
#endif // !LOG_DISABLED

    StoreDecision storeDecision = makeStoreDecision(originalRequest, response);
    if (storeDecision != StoreDecision::Yes) {
        LOG(NetworkCache, "(NetworkProcess) didn't store, storeDecision=%d", static_cast<int>(storeDecision));
        auto key = makeCacheKey(originalRequest);

        auto isSuccessfulRevalidation = response.httpStatusCode() == 304;
        if (!isSuccessfulRevalidation) {
            // Make sure we don't keep a stale entry in the cache.
            remove(key);
        }

        if (m_statistics)
            m_statistics->recordNotCachingResponse(key, storeDecision);

        return nullptr;
    }

    std::unique_ptr<Entry> cacheEntry = std::make_unique<Entry>(makeCacheKey(originalRequest), response, WTF::move(responseData), collectVaryingRequestHeaders(originalRequest, response));

    auto record = cacheEntry->encodeAsStorageRecord();

    m_storage->store(record, [completionHandler](const Data& bodyData) {
        MappedBody mappedBody;
#if ENABLE(SHAREABLE_RESOURCE)
        if (RefPtr<SharedMemory> sharedMemory = bodyData.tryCreateSharedMemory()) {
            mappedBody.shareableResource = ShareableResource::create(WTF::move(sharedMemory), 0, bodyData.size());
            ASSERT(mappedBody.shareableResource);
            mappedBody.shareableResource->createHandle(mappedBody.shareableResourceHandle);
        }
#endif
        completionHandler(mappedBody);
        LOG(NetworkCache, "(NetworkProcess) stored");
    });

    return cacheEntry;
}

std::unique_ptr<Entry> Cache::update(const WebCore::ResourceRequest& originalRequest, const GlobalFrameID& frameID, const Entry& existingEntry, const WebCore::ResourceResponse& validatingResponse)
{
    LOG(NetworkCache, "(NetworkProcess) updating %s", originalRequest.url().string().latin1().data());

    WebCore::ResourceResponse response = existingEntry.response();
    WebCore::updateResponseHeadersAfterRevalidation(response, validatingResponse);
    response.setSource(WebCore::ResourceResponse::Source::DiskCache);

    auto updateEntry = std::make_unique<Entry>(existingEntry.key(), response, existingEntry.buffer(), collectVaryingRequestHeaders(originalRequest, response));
    auto updateRecord = updateEntry->encodeAsStorageRecord();

    m_storage->store(updateRecord, { });

    if (m_statistics)
        m_statistics->recordRevalidationSuccess(frameID.first, existingEntry.key(), originalRequest);

    return updateEntry;
}

void Cache::remove(const Key& key)
{
    ASSERT(isEnabled());

    m_storage->remove(key);
}

void Cache::remove(const WebCore::ResourceRequest& request)
{
    remove(makeCacheKey(request));
}

void Cache::traverse(std::function<void (const Entry*)>&& traverseHandler)
{
    ASSERT(isEnabled());

    m_storage->traverse(resourceType(), 0, [traverseHandler](const Storage::Record* record, const Storage::RecordInfo&) {
        if (!record) {
            traverseHandler(nullptr);
            return;
        }

        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry)
            return;

        traverseHandler(entry.get());
    });
}

String Cache::dumpFilePath() const
{
    return WebCore::pathByAppendingComponent(m_storage->versionPath(), "dump.json");
}

void Cache::dumpContentsToFile()
{
    if (!m_storage)
        return;
    auto fd = WebCore::openFile(dumpFilePath(), WebCore::OpenForWrite);
    if (!WebCore::isHandleValid(fd))
        return;
    auto prologue = String("{\n\"entries\": [\n").utf8();
    WebCore::writeToFile(fd, prologue.data(), prologue.length());

    struct Totals {
        unsigned count { 0 };
        double worth { 0 };
        size_t bodySize { 0 };
    };
    Totals totals;
    auto flags = Storage::TraverseFlag::ComputeWorth | Storage::TraverseFlag::ShareCount;
    size_t capacity = m_storage->capacity();
    m_storage->traverse(resourceType(), flags, [fd, totals, capacity](const Storage::Record* record, const Storage::RecordInfo& info) mutable {
        if (!record) {
            StringBuilder epilogue;
            epilogue.appendLiteral("{}\n],\n");
            epilogue.appendLiteral("\"totals\": {\n");
            epilogue.appendLiteral("\"capacity\": ");
            epilogue.appendNumber(capacity);
            epilogue.appendLiteral(",\n");
            epilogue.appendLiteral("\"count\": ");
            epilogue.appendNumber(totals.count);
            epilogue.appendLiteral(",\n");
            epilogue.appendLiteral("\"bodySize\": ");
            epilogue.appendNumber(totals.bodySize);
            epilogue.appendLiteral(",\n");
            epilogue.appendLiteral("\"averageWorth\": ");
            epilogue.appendNumber(totals.count ? totals.worth / totals.count : 0);
            epilogue.appendLiteral("\n");
            epilogue.appendLiteral("}\n}\n");
            auto writeData = epilogue.toString().utf8();
            WebCore::writeToFile(fd, writeData.data(), writeData.length());
            WebCore::closeFile(fd);
            return;
        }
        auto entry = Entry::decodeStorageRecord(*record);
        if (!entry)
            return;
        ++totals.count;
        totals.worth += info.worth;
        totals.bodySize += info.bodySize;

        StringBuilder json;
        entry->asJSON(json, info);
        json.appendLiteral(",\n");
        auto writeData = json.toString().utf8();
        WebCore::writeToFile(fd, writeData.data(), writeData.length());
    });
}

void Cache::deleteDumpFile()
{
    auto queue = WorkQueue::create("com.apple.WebKit.Cache.delete");
    StringCapture dumpFilePathCapture(dumpFilePath());
    queue->dispatch([dumpFilePathCapture] {
        WebCore::deleteFile(dumpFilePathCapture.string());
    });
}

void Cache::clear(std::chrono::system_clock::time_point modifiedSince, std::function<void ()>&& completionHandler)
{
    LOG(NetworkCache, "(NetworkProcess) clearing cache");

    if (m_statistics)
        m_statistics->clear();

    if (!m_storage) {
        RunLoop::main().dispatch(completionHandler);
        return;
    }
    String anyType;
    m_storage->clear(anyType, modifiedSince, WTF::move(completionHandler));

    deleteDumpFile();
}

void Cache::clear()
{
    clear(std::chrono::system_clock::time_point::min(), nullptr);
}

String Cache::recordsPath() const
{
    return m_storage ? m_storage->recordsPath() : String();
}

}
}

#endif
