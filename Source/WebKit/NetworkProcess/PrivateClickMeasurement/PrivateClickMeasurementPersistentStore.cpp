/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "PrivateClickMeasurementPersistentStore.h"

#include "PrivateClickMeasurementDatabase.h"
#include "PrivateClickMeasurementDebugInfo.h"
#include "PrivateClickMeasurementManager.h"
#include <WebCore/PrivateClickMeasurement.h>
#include <wtf/RunLoop.h>
#include <wtf/SuspendableWorkQueue.h>

namespace WebKit::PCM {

static Ref<SuspendableWorkQueue> sharedWorkQueue()
{
    static NeverDestroyed<Ref<SuspendableWorkQueue>> queue(SuspendableWorkQueue::create("PrivateClickMeasurement Process Data Queue"_s,  WorkQueue::QOS::Utility));
    return queue.get().copyRef();
}

void PersistentStore::prepareForProcessToSuspend(CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    sharedWorkQueue()->suspend(Database::interruptAllDatabases, WTFMove(completionHandler));
}

void PersistentStore::processDidResume()
{
    ASSERT(RunLoop::isMain());
    sharedWorkQueue()->resume();
}

PersistentStore::PersistentStore(const String& databaseDirectory)
    : m_queue(sharedWorkQueue())
{
    if (!databaseDirectory.isEmpty()) {
        postTask([this, protectedThis = Ref { *this }, databaseDirectory = databaseDirectory.isolatedCopy()] () mutable {
            m_database = makeUnique<Database>(WTFMove(databaseDirectory));
        });
    }
}

PersistentStore::~PersistentStore() = default;

void PersistentStore::postTask(Function<void()>&& task) const
{
    ASSERT(RunLoop::isMain());
    m_queue->dispatch(WTFMove(task));
}

void PersistentStore::postTaskReply(WTF::Function<void()>&& reply) const
{
    ASSERT(!RunLoop::isMain());
    RunLoop::main().dispatch(WTFMove(reply));
}

void PersistentStore::insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&& attribution, PrivateClickMeasurementAttributionType attributionType, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, attribution = WTFMove(attribution), attributionType, completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_database)
            m_database->insertPrivateClickMeasurement(WTFMove(attribution), attributionType);
        postTaskReply(WTFMove(completionHandler));
    });
}

void PersistentStore::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting()
{
    postTask([this, protectedThis = Ref { *this }] {
        if (m_database)
            m_database->markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
    });
}

void PersistentStore::attributePrivateClickMeasurement(WebCore::PCM::SourceSite&& sourceSite, WebCore::PCM::AttributionDestinationSite&& destinationSite, const ApplicationBundleIdentifier& applicationBundleIdentifier, WebCore::PCM::AttributionTriggerData&& attributionTriggerData, WebCore::PrivateClickMeasurement::IsRunningLayoutTest isRunningTest, CompletionHandler<void(std::optional<WebCore::PCM::AttributionSecondsUntilSendData>&&, DebugInfo&&)>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, sourceSite = WTFMove(sourceSite).isolatedCopy(), destinationSite = WTFMove(destinationSite).isolatedCopy(), applicationBundleIdentifier = applicationBundleIdentifier.isolatedCopy(), attributionTriggerData = WTFMove(attributionTriggerData), isRunningTest, completionHandler = WTFMove(completionHandler)] () mutable {
        if (!m_database) {
            return postTaskReply([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(std::nullopt, { });
            });
        }

        auto [seconds, debugInfo] = m_database->attributePrivateClickMeasurement(sourceSite, destinationSite, applicationBundleIdentifier, WTFMove(attributionTriggerData), isRunningTest);

        postTaskReply([seconds = WTFMove(seconds), debugInfo = WTFMove(debugInfo).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(seconds), WTFMove(debugInfo));
        });
    });
}

void PersistentStore::privateClickMeasurementToStringForTesting(CompletionHandler<void(String)>&& completionHandler) const
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        String result;
        if (m_database)
            result = m_database->privateClickMeasurementToStringForTesting();
        postTaskReply([result = WTFMove(result).isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void PersistentStore::allAttributedPrivateClickMeasurement(CompletionHandler<void(Vector<WebCore::PrivateClickMeasurement>&&)>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<WebCore::PrivateClickMeasurement> convertedAttributions;
        if (m_database)
            convertedAttributions = m_database->allAttributedPrivateClickMeasurement();
        postTaskReply([convertedAttributions = crossThreadCopy(WTFMove(convertedAttributions)), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(convertedAttributions));
        });
    });
}

void PersistentStore::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        if (m_database)
            m_database->markAttributedPrivateClickMeasurementsAsExpiredForTesting();
        postTaskReply(WTFMove(completionHandler));
    });
}

void PersistentStore::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_database)
            m_database->clearPrivateClickMeasurement(std::nullopt);
        postTaskReply(WTFMove(completionHandler));
    });
}

void PersistentStore::clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, domain = WTFMove(domain).isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        if (m_database)
            m_database->clearPrivateClickMeasurement(domain);
        postTaskReply(WTFMove(completionHandler));
    });
}

void PersistentStore::clearExpiredPrivateClickMeasurement()
{
    postTask([this, protectedThis = Ref { *this }]() {
        if (m_database)
            m_database->clearExpiredPrivateClickMeasurement();
    });
}

void PersistentStore::clearSentAttribution(WebCore::PrivateClickMeasurement&& attributionToClear, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    postTask([this, protectedThis = Ref { *this }, attributionToClear = WTFMove(attributionToClear).isolatedCopy(), attributionReportEndpoint]() mutable {
        if (m_database)
            m_database->clearSentAttribution(WTFMove(attributionToClear), attributionReportEndpoint);
    });
}

void PersistentStore::close(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        m_database = nullptr;
        postTaskReply(WTFMove(completionHandler));
    });
}

} // namespace WebKit::PCM
