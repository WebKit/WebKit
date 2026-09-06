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
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LoaderStrategy.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "Logging.h"
#include "OwnerPermissionsPolicyData.h"
#include "PermissionsPolicy.h"
#include "PlatformStrategies.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferredFetchRegistry);

static constexpr uint64_t minimalReservedQuota = 8 * 1024;
static constexpr uint64_t normalReservedQuota = 64 * 1024;
static constexpr uint64_t topLevelReservedQuota = 512 * 1024;
static constexpr uint64_t perRequestOriginQuota = 64 * 1024;

static Ref<Document> deferredFetchControlDocument(Document& document)
{
    Ref controlDocument = document;
    while (RefPtr frame = controlDocument->frame()) {
        RefPtr parentFrame = dynamicDowncast<LocalFrame>(frame->tree().parent());
        if (!parentFrame)
            break;
        RefPtr parentDocument = parentFrame->document();
        if (!parentDocument || !protect(controlDocument->securityOrigin())->isSameOriginAs(protect(parentDocument->securityOrigin())))
            break;
        controlDocument = parentDocument.releaseNonNull();
    }
    return controlDocument;
}

static uint64_t reservedQuota(Document& controlDocument)
{
    bool deferredFetchAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetch, controlDocument, PermissionsPolicy::ShouldReportViolation::No);
    bool deferredFetchMinimalAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetchMinimal, controlDocument, PermissionsPolicy::ShouldReportViolation::No);

    if (RefPtr frame = controlDocument.frame(); frame && frame->isMainFrame())
        return deferredFetchAllowed && deferredFetchMinimalAllowed ? topLevelReservedQuota : 0;

    if (deferredFetchAllowed)
        return normalReservedQuota;
    if (!deferredFetchMinimalAllowed)
        return 0;

    if (auto ownerPolicy = controlDocument.ownerPermissionsPolicy(); ownerPolicy && ownerPolicy->containerPolicy.contains(PermissionsPolicy::Feature::DeferredFetch))
        return 0;

    return minimalReservedQuota;
}

class DeferredFetchRegistry::Record {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Record);
public:
    Record(RecordIdentifier identifier, ResourceRequest&& request, HTTPHeaderMap&& originalRequestHeaders, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, uint64_t requestBytes, SecurityOriginData reportingOrigin, Document& quotaOwnerDocument, Ref<FetchLaterResult>&& result)
        : m_identifier(identifier)
        , m_request(WTF::move(request))
        , m_originalRequestHeaders(WTF::move(originalRequestHeaders))
        , m_options(WTF::move(options))
        , m_body(WTF::move(body))
        , m_requestBytes(requestBytes)
        , m_reportingOrigin(WTF::move(reportingOrigin))
        , m_quotaOwnerDocument(quotaOwnerDocument)
        , m_result(WTF::move(result))
    {
    }

    RecordIdentifier identifier() const { return m_identifier; }
    const ResourceRequest& request() const { return m_request; }
    ResourceRequest takeRequest()
    {
        ResourceRequest request = WTF::move(m_request);
        if (m_body)
            request.setHTTPBody(m_body.copyRef());
        return request;
    }
    const HTTPHeaderMap& originalRequestHeaders() const { return m_originalRequestHeaders; }
    const ResourceLoaderOptions& options() const { return m_options; }
    uint64_t requestBytes() const { return m_requestBytes; }
    const SecurityOriginData& reportingOrigin() const { return m_reportingOrigin; }
    Document* quotaOwnerDocument() const { return m_quotaOwnerDocument.get(); }
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
    ResourceRequest m_request;
    HTTPHeaderMap m_originalRequestHeaders;
    ResourceLoaderOptions m_options;
    RefPtr<FormData> m_body;
    uint64_t m_requestBytes { 0 };
    SecurityOriginData m_reportingOrigin;
    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_quotaOwnerDocument;
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

uint64_t DeferredFetchRegistry::availableBytesFor(const SecurityOriginData& origin)
{
    Ref document = m_document.get();
    Ref quotaOwnerDocument = deferredFetchControlDocument(document);
    auto quota = reservedQuota(quotaOwnerDocument);
    auto& quotaRegistry = ensure(quotaOwnerDocument);
    auto originBytesUsed = quotaRegistry.m_bytesUsedByOrigin.get(origin);
    auto availableForOrigin = originBytesUsed >= perRequestOriginQuota ? 0 : perRequestOriginQuota - originBytesUsed;
    auto availableInReservation = quotaRegistry.m_totalBytesUsed >= quota ? 0 : quota - quotaRegistry.m_totalBytesUsed;
    return std::min(availableForOrigin, availableInReservation);
}

RefPtr<FetchLaterResult> DeferredFetchRegistry::addDeferredFetch(ResourceRequest&& request, HTTPHeaderMap&& originalRequestHeaders, ResourceLoaderOptions&& options, RefPtr<FormData>&& body, uint64_t requestBytes, AbortSignal* signal, std::optional<Seconds> activateAfter)
{
    auto reportingOrigin = SecurityOriginData::fromURL(request.url());
    if (requestBytes > availableBytesFor(reportingOrigin))
        return nullptr;

    Ref document = m_document.get();
    Ref quotaOwnerDocument = deferredFetchControlDocument(document);
    auto& quotaRegistry = ensure(quotaOwnerDocument);
    quotaRegistry.m_bytesUsedByOrigin.add(reportingOrigin, 0).iterator->value += requestBytes;
    quotaRegistry.m_totalBytesUsed += requestBytes;

    auto result = FetchLaterResult::create();
    auto identifier = m_nextRecordIdentifier++;
    m_records.append(makeUnique<Record>(identifier, WTF::move(request), WTF::move(originalRequestHeaders), WTF::move(options), WTF::move(body), requestBytes, reportingOrigin, quotaOwnerDocument, result.copyRef()));
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

    RELEASE_LOG(Loading, "[fetchLater] REGISTER id=%llu activateAfter=%d url=%s", static_cast<unsigned long long>(identifier), activateAfter ? static_cast<int>(activateAfter->milliseconds()) : -1, record.request().url().string().utf8().data());
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
    RefPtr ownerDocument = record.quotaOwnerDocument();
    auto* quotaRegistry = ownerDocument ? from(*ownerDocument) : nullptr;
    if (!quotaRegistry)
        return;

    auto iterator = quotaRegistry->m_bytesUsedByOrigin.find(record.reportingOrigin());
    if (iterator != quotaRegistry->m_bytesUsedByOrigin.end()) {
        iterator->value = record.requestBytes() > iterator->value ? 0 : iterator->value - record.requestBytes();
        if (!iterator->value)
            quotaRegistry->m_bytesUsedByOrigin.remove(iterator);
    }
    quotaRegistry->m_totalBytesUsed = record.requestBytes() > quotaRegistry->m_totalBytesUsed ? 0 : quotaRegistry->m_totalBytesUsed - record.requestBytes();
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
    platformStrategies()->loaderStrategy()->startPingLoad(*frame, request, record.originalRequestHeaders(), record.options(), record.options().contentSecurityPolicyImposition, [](const ResourceError&, const ResourceResponse&) { });

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
