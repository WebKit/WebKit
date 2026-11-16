/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DeferredFetchRegistry);

static constexpr uint64_t minimalReservedQuota = 8 * 1024;
static constexpr uint64_t normalReservedQuota = 64 * 1024;
static constexpr uint64_t topLevelQuota = 640 * 1024;
static constexpr uint64_t topLevelQuotaReservedForMinimal = 128 * 1024;
static constexpr uint64_t topLevelReservedQuota = topLevelQuota - topLevelQuotaReservedForMinimal;

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

// https://fetch.spec.whatwg.org/#get-the-available-deferred-fetch-quota
// The policy-controlled features are evaluated against the control document, not the
// document that called fetchLater().
static uint64_t reservedQuota(const Frame& controlFrame)
{
    RefPtr controlLocalFrame = dynamicDowncast<LocalFrame>(controlFrame);
    if (!controlLocalFrame)
        return 0;
    RefPtr controlDocument = controlLocalFrame->document();
    if (!controlDocument)
        return 0;

    bool deferredFetchAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetch, *controlDocument, PermissionsPolicy::ShouldReportViolation::No);
    bool deferredFetchMinimalAllowed = PermissionsPolicy::isFeatureEnabled(PermissionsPolicy::Feature::DeferredFetchMinimal, *controlDocument, PermissionsPolicy::ShouldReportViolation::No);

    if (controlFrame.isMainFrame()) {
        if (!deferredFetchAllowed)
            return 0;
        if (!deferredFetchMinimalAllowed)
            return topLevelQuota;
        return topLevelReservedQuota;
    }

    if (deferredFetchAllowed)
        return normalReservedQuota;
    if (!deferredFetchMinimalAllowed)
        return 0;

    // The spec reserves a container's quota when the container navigates, so a container
    // granted "deferred-fetch" that then commits a document which is not allowed to use the
    // feature holds normal quota rather than minimal quota, and gets nothing here. Quota is
    // only reserved at fetchLater() time for now, so approximate that by looking at the
    // container policy.
    if (auto ownerPolicy = controlDocument->ownerPermissionsPolicy(); ownerPolicy && ownerPolicy->containerPolicy.contains(PermissionsPolicy::Feature::DeferredFetch))
        return 0;

    return minimalReservedQuota;
}

class DeferredFetchRegistry::Record {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(Record);
public:
    Record(RecordIdentifier identifier, DeferredFetchIdentifier quotaIdentifier, ResourceRequest&& request, ResourceLoaderOptions&& options, Ref<FetchLaterResult>&& result)
        : m_identifier(identifier)
        , m_quotaIdentifier(quotaIdentifier)
        , m_request(WTF::move(request))
        , m_options(WTF::move(options))
        , m_result(WTF::move(result))
    {
    }

    RecordIdentifier identifier() const { return m_identifier; }
    DeferredFetchIdentifier quotaIdentifier() const { return m_quotaIdentifier; }
    const ResourceRequest& request() const { return m_request; }
    ResourceRequest takeRequest() { return WTF::move(m_request); }
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

    bool isOwnedByNetworkProcess() const { return m_ownedByNetworkProcess; }
    void setOwnedByNetworkProcess() { m_ownedByNetworkProcess = true; }

    void setActivateAfterTimer(EventLoopTimerHandle handle) { m_activateAfterTimer = handle; }
    void clearActivateAfterTimer() { m_activateAfterTimer = nullptr; }

private:
    RecordIdentifier m_identifier;
    DeferredFetchIdentifier m_quotaIdentifier;
    ResourceRequest m_request;
    ResourceLoaderOptions m_options;
    const Ref<FetchLaterResult> m_result;
    RefPtr<AbortSignal> m_abortSignal;
    std::optional<uint32_t> m_abortAlgorithmIdentifier;
    EventLoopTimerHandle m_activateAfterTimer;
    bool m_ownedByNetworkProcess { false };
};

DeferredFetchRegistry::DeferredFetchRegistry(Document& document)
    : m_document(document)
{
}

DeferredFetchRegistry::~DeferredFetchRegistry()
{
    // The fetch group is terminating, so pending records have to be sent rather than
    // discarded. Only the ones the network process holds can be: anything still owned here
    // needs a document to load through, and m_document may already be partly destroyed.
    auto records = std::exchange(m_records, { });
    for (size_t index = 0; index < records.size(); ++index) {
        auto record = WTF::move(records[index]);
        record->clearAbortAlgorithm();
        record->clearActivateAfterTimer();
        if (!record->isOwnedByNetworkProcess()) {
            releaseQuota(*record);
            continue;
        }
        record->result().setActivated();
        platformStrategies()->loaderStrategy()->sendDeferredFetch(record->quotaIdentifier());
    }
}

Ref<DeferredFetchRegistry> DeferredFetchRegistry::create(Document& document)
{
    return adoptRef(*new DeferredFetchRegistry(document));
}

RefPtr<FetchLaterResult> DeferredFetchRegistry::addDeferredFetch(ResourceRequest&& request, ResourceLoaderOptions&& options, uint64_t requestBytes, RefPtr<AbortSignal>&& signal, std::optional<Seconds> activateAfter, uint64_t& availableBytes)
{
    ASSERT(RunLoop::isMain());

    Ref document { m_document };
    RefPtr frame = deferredFetchControlFrame(document);
    if (!frame) {
        availableBytes = 0;
        return nullptr;
    }

    auto reportingOrigin = SecurityOriginData::fromURL(request.url());
    Ref localFrame = *document->frame();
    auto [quotaIdentifier, remainingBytes] = platformStrategies()->loaderStrategy()->reserveDeferredFetchQuota(localFrame, frame->frameID(), reportingOrigin, reservedQuota(*frame), frame->isMainFrame(), requestBytes);
    availableBytes = remainingBytes;
    if (!quotaIdentifier)
        return nullptr;

    // Hand the request to the network process straight away so that it can be sent even if
    // activation happens because the document is going away, when no loader is left here.
    bool ownedByNetworkProcess = platformStrategies()->loaderStrategy()->addDeferredFetch(localFrame, *quotaIdentifier, request, options);

    auto result = FetchLaterResult::create();
    auto identifier = RecordIdentifier::generate();
    auto newRecord = makeUnique<Record>(identifier, *quotaIdentifier, WTF::move(request), WTF::move(options), result.copyRef());
    if (ownedByNetworkProcess)
        newRecord->setOwnedByNetworkProcess();
    if (signal && !signal->aborted()) {
        RefPtr protectedSignal = WTF::move(signal);
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document };
        auto algorithmIdentifier = protectedSignal->addAlgorithm([weakDocument, identifier](JSC::JSValue) {
            RefPtr document { weakDocument };
            if (!document)
                return;
            if (RefPtr registry = document->deferredFetchRegistryIfExists())
                registry->removeRecord(identifier);
        });
        newRecord->setAbortAlgorithmIdentifier(WTF::move(protectedSignal), algorithmIdentifier);
    }

    RELEASE_LOG(Loading, "[fetchLater] REGISTER id=%llu activateAfter=%d url=%" SENSITIVE_LOG_STRING, static_cast<unsigned long long>(identifier.toUInt64()), activateAfter ? static_cast<int>(activateAfter->milliseconds()) : -1, newRecord->request().url().string().utf8());
    if (activateAfter) {
        WeakPtr<Document, WeakPtrImplWithEventTargetData> weakDocument { m_document };
        auto handle = protect(Ref { m_document }->eventLoop())->scheduleTask(*activateAfter, TaskSource::Networking, [weakDocument, identifier] {
            RefPtr document { weakDocument };
            if (!document)
                return;
            if (RefPtr registry = document->deferredFetchRegistryIfExists())
                registry->activateRecord(identifier);
        });
        newRecord->setActivateAfterTimer(handle);
    }

    // Everything is set up before the record goes into the vector, so no reference into
    // it is held while script can run.
    m_records.append(WTF::move(newRecord));

    return result;
}

void DeferredFetchRegistry::releaseQuota(const Record& record)
{
    if (record.isOwnedByNetworkProcess()) {
        // Dropping the stored request releases its reservation too.
        platformStrategies()->loaderStrategy()->removeDeferredFetch(record.quotaIdentifier());
        return;
    }
    platformStrategies()->loaderStrategy()->releaseDeferredFetchQuota(record.quotaIdentifier());
}

void DeferredFetchRegistry::removeRecord(RecordIdentifier identifier)
{
    auto index = m_records.findIf([identifier](auto& record) {
        return record->identifier() == identifier;
    });
    if (index == notFound)
        return;

    auto record = WTF::move(m_records[index]);
    m_records.removeAt(index);
    record->clearAbortAlgorithm();
    record->clearActivateAfterTimer();
    releaseQuota(*record);
}

void DeferredFetchRegistry::activateRecord(RecordIdentifier identifier)
{
    auto index = m_records.findIf([identifier](auto& record) {
        return record->identifier() == identifier;
    });
    if (index == notFound)
        return;

    // Own the record before activating it, since sending can run script that reaches the
    // registry again.
    auto record = WTF::move(m_records[index]);
    m_records.removeAt(index);

    record->clearAbortAlgorithm();
    record->clearActivateAfterTimer();

    if (record->isOwnedByNetworkProcess()) {
        record->result().setActivated();
        // The network process already holds the request, so this works even while the
        // document is being torn down. It also releases the quota reservation.
        platformStrategies()->loaderStrategy()->sendDeferredFetch(record->quotaIdentifier());
        return;
    }

    Ref document { m_document };
    RefPtr frame = document->frame();
    if (!frame) {
        releaseQuota(*record);
        return;
    }

    record->result().setActivated();

    ResourceRequest request = record->takeRequest();
    PingLoader::startDeferredFetch(*frame, request, record->options());

    releaseQuota(*record);
}

void DeferredFetchRegistry::activateAllRecords()
{
    ASSERT(RunLoop::isMain());

    while (!m_records.isEmpty())
        activateRecord(m_records.first()->identifier());
}

void DeferredFetchRegistry::documentIsAboutToEnterBackForwardCache()
{
    activateAllRecords();
}

void DeferredFetchRegistry::activateAllDeferredFetches()
{
    activateAllRecords();
}

void DeferredFetchRegistry::documentIsBeingDestroyed()
{
    activateAllRecords();
}

} // namespace WebCore
