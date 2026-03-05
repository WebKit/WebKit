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
    sharedWorkQueue()->suspend(Database::interruptAllDatabases, WTF::move(completionHandler));
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
            m_database = Database::create(WTF::move(databaseDirectory));
        });
    }
}

PersistentStore::~PersistentStore() = default;

void PersistentStore::postTask(Function<void()>&& task) const
{
    ASSERT(RunLoop::isMain());
    m_queue->dispatch(WTF::move(task));
}

void PersistentStore::postTaskReply(WTF::Function<void()>&& reply) const
{
    ASSERT(!RunLoop::isMain());
    RunLoop::mainSingleton().dispatch(WTF::move(reply));
}

void PersistentStore::insertPrivateClickMeasurement(WebCore::PrivateClickMeasurement&& attribution, PrivateClickMeasurementAttributionType attributionType, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, attribution = WTF::move(attribution), attributionType, completionHandler = WTF::move(completionHandler)] () mutable {
        if (RefPtr database = m_database)
            database->insertPrivateClickMeasurement(WTF::move(attribution), attributionType);
        postTaskReply(WTF::move(completionHandler));
    });
}

void PersistentStore::markAllUnattributedPrivateClickMeasurementAsExpiredForTesting()
{
    postTask([this, protectedThis = Ref { *this }] {
        if (RefPtr database = m_database)
            database->markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
    });
}

void PersistentStore::attributePrivateClickMeasurement(WebCore::PCM::SourceSite&& sourceSite, WebCore::PCM::AttributionDestinationSite&& destinationSite, const ApplicationBundleIdentifier& applicationBundleIdentifier, WebCore::PCM::AttributionTriggerData&& attributionTriggerData, WebCore::PrivateClickMeasurement::IsRunningLayoutTest isRunningTest, CompletionHandler<void(std::optional<WebCore::PCM::AttributionSecondsUntilSendData>&&, DebugInfo&&)>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, sourceSite = WTF::move(sourceSite).isolatedCopy(), destinationSite = WTF::move(destinationSite).isolatedCopy(), applicationBundleIdentifier = applicationBundleIdentifier.isolatedCopy(), attributionTriggerData = WTF::move(attributionTriggerData), isRunningTest, completionHandler = WTF::move(completionHandler)] () mutable {
        RefPtr database = m_database;
        if (!database) {
            return postTaskReply([completionHandler = WTF::move(completionHandler)] () mutable {
                completionHandler(std::nullopt, { });
            });
        }

        auto [seconds, debugInfo] = database->attributePrivateClickMeasurement(sourceSite, destinationSite, applicationBundleIdentifier, WTF::move(attributionTriggerData), isRunningTest);

        postTaskReply([seconds = WTF::move(seconds), debugInfo = WTF::move(debugInfo).isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(seconds), WTF::move(debugInfo));
        });
    });
}

void PersistentStore::privateClickMeasurementToStringForTesting(CompletionHandler<void(String)>&& completionHandler) const
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        String result;
        if (RefPtr database = m_database)
            result = database->privateClickMeasurementToStringForTesting();
        postTaskReply([result = WTF::move(result).isolatedCopy(), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(result);
        });
    });
}

void PersistentStore::allAttributedPrivateClickMeasurement(CompletionHandler<void(Vector<WebCore::PrivateClickMeasurement>&&)>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        Vector<WebCore::PrivateClickMeasurement> convertedAttributions;
        if (RefPtr database = m_database)
            convertedAttributions = database->allAttributedPrivateClickMeasurement();
        postTaskReply([convertedAttributions = crossThreadCopy(WTF::move(convertedAttributions)), completionHandler = WTF::move(completionHandler)]() mutable {
            completionHandler(WTF::move(convertedAttributions));
        });
    });
}

void PersistentStore::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)]() mutable {
        if (RefPtr database = m_database)
            database->markAttributedPrivateClickMeasurementsAsExpiredForTesting();
        postTaskReply(WTF::move(completionHandler));
    });
}

void PersistentStore::clearPrivateClickMeasurement(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] () mutable {
        if (RefPtr database = m_database)
            database->clearPrivateClickMeasurement(std::nullopt);
        postTaskReply(WTF::move(completionHandler));
    });
}

void PersistentStore::clearPrivateClickMeasurementForRegistrableDomain(WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, domain = WTF::move(domain).isolatedCopy(), completionHandler = WTF::move(completionHandler)] () mutable {
        if (RefPtr database = m_database)
            database->clearPrivateClickMeasurement(domain);
        postTaskReply(WTF::move(completionHandler));
    });
}

void PersistentStore::clearExpiredPrivateClickMeasurement()
{
    postTask([this, protectedThis = Ref { *this }]() {
        if (RefPtr database = m_database)
            database->clearExpiredPrivateClickMeasurement();
    });
}

void PersistentStore::clearSentAttribution(WebCore::PrivateClickMeasurement&& attributionToClear, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    postTask([this, protectedThis = Ref { *this }, attributionToClear = WTF::move(attributionToClear).isolatedCopy(), attributionReportEndpoint]() mutable {
        if (RefPtr database = m_database)
            database->clearSentAttribution(WTF::move(attributionToClear), attributionReportEndpoint);
    });
}

void PersistentStore::close(CompletionHandler<void()>&& completionHandler)
{
    postTask([this, protectedThis = Ref { *this }, completionHandler = WTF::move(completionHandler)] () mutable {
        m_database = nullptr;
        postTaskReply(WTF::move(completionHandler));
    });
}

} // namespace WebKit::PCM
