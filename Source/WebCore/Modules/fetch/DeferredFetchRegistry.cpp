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
#include "Document.h"
#include "EventLoop.h"
#include "FetchLaterResult.h"
#include "FormData.h"
#include "Frame.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Logging.h"
#include "OwnerPermissionsPolicyData.h"
#include "PermissionsPolicy.h"
#include "PingLoader.h"
#include "PlatformStrategies.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferredFetchRegistry);

static constexpr uint64_t minimalReservedQuota = 8 * 1024;
static constexpr uint64_t normalReservedQuota = 64 * 1024;
static constexpr uint64_t topLevelReservedQuota = 512 * 1024;
static RefPtr<Frame> deferredFetchControlFrame(Document& document)
{
    RefPtr<Frame> controlFrame = document.frame();
    if (!controlFrame)
        return nullptr;

    Ref documentOrigin = document.securityOrigin();
    while (RefPtr parentFrame = controlFrame->tree().parent()) {
        RefPtr parentOrigin = parentFrame->frameDocumentSecurityOrigin();
        if (!parentOrigin || !documentOrigin->isSameOriginAs(*parentOrigin))
            break;
        controlFrame = WTF::move(parentFrame);
    }
    return controlFrame;
}

static uint64_t reservedQuota(Document& document, const Frame& controlFrame)
{
    bool deferredFetchAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetch, document, PermissionsPolicy::ShouldReportViolation::No);
    bool deferredFetchMinimalAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetchMinimal, document, PermissionsPolicy::ShouldReportViolation::No);

    if (controlFrame.isMainFrame())
        return deferredFetchAllowed && deferredFetchMinimalAllowed ? topLevelReservedQuota : 0;

    if (deferredFetchAllowed)
        return normalReservedQuota;
    if (!deferredFetchMinimalAllowed)
        return 0;

    if (auto ownerPolicy = document.ownerPermissionsPolicy(); ownerPolicy && ownerPolicy->containerPolicy.contains(PermissionsPolicy::Feature::DeferredFetch))
        return 0;

    return minimalReservedQuota;
}

class DeferredFetchRegistry::Record {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Record);
public:
    Record(RecordIdentifier identifier, uint64_t quotaIdentifier, ResourceRequest&& request, ResourceRequest&& requestForCachedResourceLoader, HTTPHeaderMap&& originalRequestHeaders, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, Ref<FetchLaterResult>&& result)
        : m_identifier(identifier)
        , m_quotaIdentifier(quotaIdentifier)
        , m_request(WTF::move(request))
        , m_requestForCachedResourceLoader(WTF::move(requestForCachedResourceLoader))
        , m_originalRequestHeaders(WTF::move(originalRequestHeaders))
        , m_options(WTF::move(options))
        , m_body(WTF::move(body))
        , m_result(WTF::move(result))
    {
    }

    RecordIdentifier identifier() const { return m_identifier; }
    uint64_t quotaIdentifier() const { return m_quotaIdentifier; }
    const ResourceRequest& request() const { return m_request; }
    ResourceRequest takeRequest()
    {
        ResourceRequest request = WTF::move(m_request);
        if (m_body)
            request.setHTTPBody(m_body.copyRef());
        return request;
    }
    ResourceRequest takeRequestForCachedResourceLoader()
    {
        ResourceRequest request = WTF::move(m_requestForCachedResourceLoader);
        if (m_body)
            request.setHTTPBody(m_body.copyRef());
        return request;
    }
    const HTTPHeaderMap& originalRequestHeaders() const { return m_originalRequestHeaders; }
    const ResourceLoaderOptions& options() const { return m_options; }
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

private:
    RecordIdentifier m_identifier;
    uint64_t m_quotaIdentifier;
    ResourceRequest m_request;
    ResourceRequest m_requestForCachedResourceLoader;
    HTTPHeaderMap m_originalRequestHeaders;
    ResourceLoaderOptions m_options;
    RefPtr<FormData> m_body;
    const Ref<FetchLaterResult> m_result;
    RefPtr<AbortSignal> m_abortSignal;
    std::optional<uint32_t> m_abortAlgorithmIdentifier;
    EventLoopTimerHandle m_activateAfterTimer;
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
    while (!m_records.isEmpty())
        removeRecord(m_records.last()->identifier());
}

DeferredFetchRegistry* DeferredFetchRegistry::from(Document& document)
{
    return downcast<DeferredFetchRegistry>(Supplement<Document>::from(&document, supplementName()));
}

DeferredFetchRegistry& DeferredFetchRegistry::ensure(Document& document)
{
    if (auto* existing = from(document))
        return *existing;
    auto newSupplement = makeUniqueWithoutRefCountedCheck<DeferredFetchRegistry>(document);
    auto& supplement = *newSupplement;
    provideTo(&document, supplementName(), WTF::move(newSupplement));
    return supplement;
}

RefPtr<FetchLaterResult> DeferredFetchRegistry::addDeferredFetch(ResourceRequest&& request, ResourceRequest&& requestForCachedResourceLoader, HTTPHeaderMap&& originalRequestHeaders, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, uint64_t requestBytes, AbortSignal* signal, std::optional<Seconds> activateAfter, uint64_t& availableBytes)
{
    Ref document = m_document.get();
    RefPtr frame = deferredFetchControlFrame(document);
    if (!frame) {
        availableBytes = 0;
        return nullptr;
    }

    auto reportingOrigin = SecurityOriginData::fromURL(request.url());
    Ref localFrame = *document->frame();
    auto [quotaIdentifier, remainingBytes] = platformStrategies()->loaderStrategy()->reserveDeferredFetchQuota(localFrame, frame->frameID(), reportingOrigin, reservedQuota(document, *frame), requestBytes);
    availableBytes = remainingBytes;
    if (!quotaIdentifier)
        return nullptr;

    auto result = FetchLaterResult::create();
    auto identifier = RecordIdentifier::generate();
    m_records.append(makeUnique<Record>(identifier, *quotaIdentifier, WTF::move(request), WTF::move(requestForCachedResourceLoader), WTF::move(originalRequestHeaders), WTF::move(options), WTF::move(body), result.copyRef()));
    auto& record = *m_records.last();

    if (signal && !signal->aborted()) {
        RefPtr protectedSignal = signal;
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document.get() };
        auto algorithmIdentifier = protectedSignal->addAlgorithm([weakDocument, identifier](JSC::JSValue) {
            RefPtr document = weakDocument.get();
            if (!document)
                return;
            if (auto* registry = DeferredFetchRegistry::from(*document))
                registry->removeRecord(identifier);
        });
        record.setAbortAlgorithmIdentifier(WTF::move(protectedSignal), algorithmIdentifier);
    }

    RELEASE_LOG(Loading, "[fetchLater] REGISTER id=%llu activateAfter=%d url=%s", static_cast<unsigned long long>(identifier.toUInt64()), activateAfter ? static_cast<int>(activateAfter->milliseconds()) : -1, record.request().url().string().utf8().data());
    if (activateAfter) {
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document.get() };
        auto handle = protect(Ref { m_document.get() }->eventLoop())->scheduleTask(*activateAfter, TaskSource::Networking, [weakDocument, identifier] {
            RefPtr document = weakDocument.get();
            if (!document)
                return;
            if (auto* registry = DeferredFetchRegistry::from(*document))
                registry->activateRecord(identifier);
        });
        record.setActivateAfterTimer(handle);
    }

    return result;
}

void DeferredFetchRegistry::releaseQuota(const Record& record)
{
    platformStrategies()->loaderStrategy()->releaseDeferredFetchQuota(record.quotaIdentifier());
}

void DeferredFetchRegistry::removeRecord(RecordIdentifier identifier)
{
    auto index = m_records.findIf([identifier](auto& record) {
        return record->identifier() == identifier;
    });
    if (index == notFound)
        return;

    auto& record = *m_records[index];
    record.clearAbortAlgorithm();
    record.clearActivateAfterTimer();
    releaseQuota(record);
    m_records.removeAt(index);
}

void DeferredFetchRegistry::activateRecord(RecordIdentifier identifier)
{
    auto index = m_records.findIf([identifier](auto& record) {
        return record->identifier() == identifier;
    });
    if (index == notFound)
        return;

    auto& record = *m_records[index];
    Ref document = m_document.get();
    RefPtr frame = document->frame();
    if (!frame) {
        removeRecord(identifier);
        return;
    }

    record.clearAbortAlgorithm();
    record.clearActivateAfterTimer();
    record.result().setActivated();

    ResourceRequest request = record.takeRequest();
    ResourceRequest requestForCachedResourceLoader = record.takeRequestForCachedResourceLoader();
    PingLoader::startDeferredFetch(*frame, request, requestForCachedResourceLoader, record.originalRequestHeaders(), record.options());

    releaseQuota(record);
    m_records.removeAt(index);
}

void DeferredFetchRegistry::activateAllRecords()
{
    while (!m_records.isEmpty())
        activateRecord(m_records.first()->identifier());
}

void DeferredFetchRegistry::documentIsAboutToEnterBackForwardCache()
{
    activateAllRecords();
}

void DeferredFetchRegistry::documentIsBeingDestroyed()
{
    activateAllRecords();
}

} // namespace WebCore
