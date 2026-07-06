/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "DeferredFetchRegistry.h"

#include "AbortSignal.h"
#include "CachedRawResource.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentResourceLoader.h"
#include "EventLoop.h"
#include "FetchLaterResult.h"
#include "FormData.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "PlatformStrategies.h"
#include "ResourceError.h"
#include "ResourceLoaderIdentifier.h"
#include "ResourceLoaderOptions.h"
#include "SecurityOriginData.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferredFetchRegistry);

// Per-request record. Owns the frozen ResourceRequest/options until activation.
class DeferredFetchRegistry::Record {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Record);
public:
    Record(ResourceRequest&& request, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, uint64_t requestBytes, SecurityOriginData reportingOrigin, Ref<FetchLaterResult>&& result)
        : m_request(WTF::move(request))
        , m_options(WTF::move(options))
        , m_body(WTF::move(body))
        , m_requestBytes(requestBytes)
        , m_reportingOrigin(WTF::move(reportingOrigin))
        , m_result(WTF::move(result))
    { }

    const ResourceRequest& request() const { return m_request; }
    ResourceRequest takeRequest()
    {
        ResourceRequest request = WTF::move(m_request);
        if (m_body)
            request.setHTTPBody(m_body.copyRef());
        return request;
    }
    const ResourceLoaderOptions& options() const { return m_options; }
    uint64_t requestBytes() const { return m_requestBytes; }
    const SecurityOriginData& reportingOrigin() const { return m_reportingOrigin; }
    FetchLaterResult& result() { return m_result.get(); }

    void setAbortAlgorithmIdentifier(RefPtr<AbortSignal>&& signal, uint32_t identifier)
    {
        m_abortSignal = WTF::move(signal);
        m_abortAlgorithmIdentifier = identifier;
    }
    void clearAbortAlgorithm()
    {
        if (RefPtr signal = WTF::move(m_abortSignal); signal && m_abortAlgorithmIdentifier)
            signal->removeAlgorithm(*m_abortAlgorithmIdentifier);
        m_abortAlgorithmIdentifier = std::nullopt;
    }

    void setActivateAfterTimer(EventLoopTimerHandle handle) { m_activateAfterTimer = handle; }
    void clearActivateAfterTimer() { m_activateAfterTimer = nullptr; }

    void setInflightResource(CachedResourceHandle<CachedResource>&& resource) { m_inflightResource = WTF::move(resource); }
    CachedResource* inflightResource() const { return m_inflightResource.get(); }

private:
    ResourceRequest m_request;
    ResourceLoaderOptions m_options;
    RefPtr<FormData> m_body;
    uint64_t m_requestBytes { 0 };
    SecurityOriginData m_reportingOrigin;
    const Ref<FetchLaterResult> m_result;

    RefPtr<AbortSignal> m_abortSignal;
    std::optional<uint32_t> m_abortAlgorithmIdentifier;

    EventLoopTimerHandle m_activateAfterTimer;
    CachedResourceHandle<CachedResource> m_inflightResource;
};

ASCIILiteral DeferredFetchRegistry::supplementName()
{
    return "DeferredFetchRegistry"_s;
}

DeferredFetchRegistry::DeferredFetchRegistry(Document& document)
    : m_document(document)
{
}

DeferredFetchRegistry::~DeferredFetchRegistry()
{
    for (auto& record : m_records) {
        if (RefPtr resource = record->inflightResource())
            resource->removeClient(*this);
        record->clearAbortAlgorithm();
        record->clearActivateAfterTimer();
    }
}

void DeferredFetchRegistry::ref() const
{
    m_document->ref();
}

void DeferredFetchRegistry::deref() const
{
    m_document->deref();
}

RefPtr<DeferredFetchRegistry> DeferredFetchRegistry::from(Document& document)
{
    return downcast<DeferredFetchRegistry>(Supplement<Document>::from(&document, supplementName()));
}

Ref<DeferredFetchRegistry> DeferredFetchRegistry::ensure(Document& document)
{
    if (RefPtr existing = from(document))
        return existing.releaseNonNull();
    auto newSupplement = makeUniqueWithoutRefCountedCheck<DeferredFetchRegistry>(document);
    Ref ref = *newSupplement;
    provideTo(&document, supplementName(), WTF::move(newSupplement));
    return ref;
}

uint64_t DeferredFetchRegistry::availableBytesFor(const SecurityOriginData& origin) const
{
    auto used = m_bytesUsedByOrigin.get(origin);
    return used >= QUOTA_BYTES_PER_ORIGIN ? 0 : QUOTA_BYTES_PER_ORIGIN - used;
}

RefPtr<FetchLaterResult> DeferredFetchRegistry::addDeferredFetch(ResourceRequest&& request, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, uint64_t requestBytes, AbortSignal* signal, std::optional<Seconds> activateAfter)
{
    auto reportingOrigin = SecurityOriginData::fromURL(request.url());
    if (requestBytes > availableBytesFor(reportingOrigin))
        return nullptr;

    auto result = FetchLaterResult::create();
    auto record = makeUnique<Record>(WTF::move(request), WTF::move(options), WTF::move(body), requestBytes, reportingOrigin, result.copyRef());
    Record* recordPtr = record.get();
    m_records.append(WTF::move(record));
    m_bytesUsedByOrigin.add(reportingOrigin, 0).iterator->value += requestBytes;

    // Wire up AbortSignal: if the signal fires, drop the record without dispatching.
    // Capture a WeakPtr<Document> and re-lookup the registry to avoid resurrecting a torn-down supplement.
    if (signal && !signal->aborted()) {
        RefPtr<AbortSignal> refSignal = signal;
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document.get() };
        auto identifier = refSignal->addAlgorithm([weakDocument, recordPtr](JSC::JSValue) {
            RefPtr document = weakDocument.get();
            if (!document)
                return;
            if (RefPtr registry = DeferredFetchRegistry::from(*document))
                registry->removeRecord(*recordPtr);
        });
        recordPtr->setAbortAlgorithmIdentifier(WTF::move(refSignal), identifier);
    }

    // activateAfter timer, if requested.
    if (activateAfter) {
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document.get() };
        auto handle = protect(Ref { m_document.get() }->eventLoop())->scheduleTask(*activateAfter, TaskSource::Networking, [weakDocument, recordPtr] {
            RefPtr document = weakDocument.get();
            if (!document)
                return;
            if (RefPtr registry = DeferredFetchRegistry::from(*document))
                registry->activateRecord(*recordPtr);
        });
        recordPtr->setActivateAfterTimer(handle);
    }

    return result;
}

void DeferredFetchRegistry::removeRecord(Record& record)
{
    record.clearAbortAlgorithm();
    record.clearActivateAfterTimer();

    if (RefPtr resource = record.inflightResource())
        resource->removeClient(*this);

    auto it = m_bytesUsedByOrigin.find(record.reportingOrigin());
    if (it != m_bytesUsedByOrigin.end()) {
        it->value = record.requestBytes() > it->value ? 0 : it->value - record.requestBytes();
        if (!it->value)
            m_bytesUsedByOrigin.remove(it);
    }

    m_records.removeFirstMatching([&](auto& entry) {
        return entry.get() == &record;
    });
}

void DeferredFetchRegistry::activateRecord(Record& record)
{
    // Already dispatched?
    if (record.inflightResource())
        return;

    Ref document = m_document.get();
    RefPtr frame = document->frame();
    if (!frame) {
        // Document is fully detached — can no longer dispatch.
        removeRecord(record);
        return;
    }

    record.clearAbortAlgorithm();
    record.clearActivateAfterTimer();
    record.result().setActivated();

    ResourceRequest request = record.takeRequest();
    const ResourceLoaderOptions& options = record.options();

    // Dispatch directly via the LoaderStrategy's ping-load path — same as
    // PingLoader / sendBeacon in its ping-load fallback. This bypasses
    // CachedResourceLoader, which may already have had its DocumentLoader
    // cleared by the time we activate on document destruction. The load's
    // keepAlive flag will cause NetworkResourceLoader::abort() to transfer
    // it to the network process's keep-alive pool if the process is torn
    // down before completion.
    HTTPHeaderMap originalHeaders = request.httpHeaderFields();
    platformStrategies()->loaderStrategy()->startPingLoad(*frame, request, originalHeaders, options, options.contentSecurityPolicyImposition, [](const ResourceError&, const ResourceResponse&) {
        // Response is intentionally dropped: fetchLater exposes nothing to JS.
    });

    // We don't hold a CachedResourceHandle for LoaderStrategy-dispatched
    // loads; the network process owns them. Refund quota now.
    auto it = m_bytesUsedByOrigin.find(record.reportingOrigin());
    if (it != m_bytesUsedByOrigin.end()) {
        it->value = record.requestBytes() > it->value ? 0 : it->value - record.requestBytes();
        if (!it->value)
            m_bytesUsedByOrigin.remove(it);
    }
    m_records.removeFirstMatching([&](auto& entry) {
        return entry.get() == &record;
    });
}

void DeferredFetchRegistry::activateAllRecords()
{
    // Snapshot pointers because activation may mutate m_records
    // (via early failure paths).
    Vector<Record*> snapshot;
    snapshot.reserveInitialCapacity(m_records.size());
    for (auto& record : m_records)
        snapshot.append(record.get());

    for (auto* record : snapshot)
        activateRecord(*record);
}

void DeferredFetchRegistry::documentIsAboutToEnterBackForwardCache()
{
    activateAllRecords();
}

void DeferredFetchRegistry::documentIsBeingDestroyed()
{
    activateAllRecords();
}

void DeferredFetchRegistry::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&, LoadWillContinueInAnotherProcess)
{
    resource.removeClient(*this);

    auto index = m_records.findIf([&](auto& entry) {
        return entry->inflightResource() == &resource;
    });
    if (index == notFound)
        return;

    auto& record = m_records[index];
    auto it = m_bytesUsedByOrigin.find(record->reportingOrigin());
    if (it != m_bytesUsedByOrigin.end()) {
        it->value = record->requestBytes() > it->value ? 0 : it->value - record->requestBytes();
        if (!it->value)
            m_bytesUsedByOrigin.remove(it);
    }
    m_records.removeAt(index);
}

} // namespace WebCore
