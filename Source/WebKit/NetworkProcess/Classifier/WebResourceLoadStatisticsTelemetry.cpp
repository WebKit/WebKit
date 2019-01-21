/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WebResourceLoadStatisticsTelemetry.h"

#if ENABLE(RESOURCE_LOAD_STATISTICS)

#include "ResourceLoadStatisticsMemoryStore.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include <WebCore/DiagnosticLoggingKeys.h>
#include <WebCore/ResourceLoadStatistics.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

const unsigned minimumPrevalentResourcesForTelemetry = 3;
const unsigned significantFiguresForLoggedValues = 3;
static bool notifyPagesWhenTelemetryWasCaptured = false;

struct PrevalentResourceTelemetry {
    unsigned numberOfTimesDataRecordsRemoved;
    bool hasHadUserInteraction;
    unsigned daysSinceUserInteraction;
    unsigned subframeUnderTopFrameOrigins;
    unsigned subresourceUnderTopFrameOrigins;
    unsigned subresourceUniqueRedirectsTo;
    unsigned timesAccessedAsFirstPartyDueToUserInteraction;
    unsigned timesAccessedAsFirstPartyDueToStorageAccessAPI;
};

static Vector<PrevalentResourceTelemetry> sortedPrevalentResourceTelemetry(const ResourceLoadStatisticsMemoryStore& store)
{
    ASSERT(!RunLoop::isMain());
    Vector<PrevalentResourceTelemetry> sorted;
    store.processStatistics([&sorted] (auto& statistic) {
        if (!statistic.isPrevalentResource)
            return;

        unsigned daysSinceUserInteraction = statistic.mostRecentUserInteractionTime <= WallTime() ? 0 : std::floor((WallTime::now() - statistic.mostRecentUserInteractionTime) / 24_h);
        sorted.append(PrevalentResourceTelemetry {
            statistic.dataRecordsRemoved,
            statistic.hadUserInteraction,
            daysSinceUserInteraction,
            statistic.subframeUnderTopFrameOrigins.size(),
            statistic.subresourceUnderTopFrameOrigins.size(),
            statistic.subresourceUniqueRedirectsTo.size(),
            statistic.timesAccessedAsFirstPartyDueToUserInteraction,
            statistic.timesAccessedAsFirstPartyDueToStorageAccessAPI
        });
    });

    if (sorted.size() < minimumPrevalentResourcesForTelemetry)
        return { };

    std::sort(sorted.begin(), sorted.end(), [](const PrevalentResourceTelemetry& a, const PrevalentResourceTelemetry& b) {
        return a.subframeUnderTopFrameOrigins + a.subresourceUnderTopFrameOrigins + a.subresourceUniqueRedirectsTo >
        b.subframeUnderTopFrameOrigins + b.subresourceUnderTopFrameOrigins + b.subresourceUniqueRedirectsTo;
    });

    return sorted;
}

static unsigned numberOfResourcesWithUserInteraction(const Vector<PrevalentResourceTelemetry>& resources, size_t begin, size_t end)
{
    if (resources.isEmpty() || resources.size() < begin + 1 || resources.size() < end + 1)
        return 0;
    
    unsigned result = 0;
    for (size_t i = begin; i < end; ++i) {
        if (resources[i].hasHadUserInteraction)
            ++result;
    }
    
    return result;
}
    
static unsigned median(const Vector<unsigned>& v)
{
    if (v.isEmpty())
        return 0;
    if (v.size() == 1)
        return v[0];
    
    auto size = v.size();
    auto middle = size / 2;
    if (size % 2)
        return v[middle];
    return (v[middle - 1] + v[middle]) / 2;
}
    
static unsigned median(const Vector<PrevalentResourceTelemetry>& v, unsigned begin, unsigned end, const WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)>& statisticGetter)
{
    if (v.isEmpty() || v.size() < begin + 1 || v.size() < end + 1)
        return 0;
    
    Vector<unsigned> part;
    part.reserveInitialCapacity(end - begin + 1);
    for (unsigned i = begin; i <= end; ++i)
        part.uncheckedAppend(statisticGetter(v[i]));
    
    return median(part);
}
    
static void submitTopList(unsigned numberOfResourcesFromTheTop, const Vector<PrevalentResourceTelemetry>& sortedPrevalentResources, const Vector<PrevalentResourceTelemetry>& sortedPrevalentResourcesWithoutUserInteraction, const WebResourceLoadStatisticsStore& store)
{
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> subframeUnderTopFrameOriginsGetter = [] (auto& t) {
        return t.subframeUnderTopFrameOrigins;
    };
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> subresourceUnderTopFrameOriginsGetter = [] (auto& t) {
        return t.subresourceUnderTopFrameOrigins;
    };
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> subresourceUniqueRedirectsToGetter = [] (auto& t) {
        return t.subresourceUniqueRedirectsTo;
    };
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> numberOfTimesDataRecordsRemovedGetter = [] (auto& t) {
        return t.numberOfTimesDataRecordsRemoved;
    };
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> numberOfTimesAccessedAsFirstPartyDueToUserInteractionGetter = [] (auto& t) {
        return t.timesAccessedAsFirstPartyDueToUserInteraction;
    };
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> numberOfTimesAccessedAsFirstPartyDueToStorageAccessAPIGetter = [] (auto& t) {
        return t.timesAccessedAsFirstPartyDueToStorageAccessAPI;
    };

    unsigned topPrevalentResourcesWithUserInteraction = numberOfResourcesWithUserInteraction(sortedPrevalentResources, 0, numberOfResourcesFromTheTop - 1);
    unsigned topSubframeUnderTopFrameOrigins = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, subframeUnderTopFrameOriginsGetter);
    unsigned topSubresourceUnderTopFrameOrigins = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, subresourceUnderTopFrameOriginsGetter);
    unsigned topSubresourceUniqueRedirectsTo = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, subresourceUniqueRedirectsToGetter);
    unsigned topNumberOfTimesDataRecordsRemoved = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, numberOfTimesDataRecordsRemovedGetter);
    unsigned topNumberOfTimesAccessedAsFirstPartyDueToUserInteraction = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, numberOfTimesAccessedAsFirstPartyDueToUserInteractionGetter);
    unsigned topNumberOfTimesAccessedAsFirstPartyDueToStorageAccessAPI = median(sortedPrevalentResourcesWithoutUserInteraction, 0, numberOfResourcesFromTheTop - 1, numberOfTimesAccessedAsFirstPartyDueToStorageAccessAPIGetter);

    StringBuilder preambleBuilder;
    preambleBuilder.appendLiteral("top");
    preambleBuilder.appendNumber(numberOfResourcesFromTheTop);
    String descriptionPreamble = preambleBuilder.toString();
    
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "PrevalentResourcesWithUserInteraction",
        topPrevalentResourcesWithUserInteraction, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "SubframeUnderTopFrameOrigins",
        topSubframeUnderTopFrameOrigins, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "SubresourceUnderTopFrameOrigins",
        topSubresourceUnderTopFrameOrigins, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "SubresourceUniqueRedirectsTo",
        topSubresourceUniqueRedirectsTo, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "NumberOfTimesDataRecordsRemoved",
        topNumberOfTimesDataRecordsRemoved, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "NumberOfTimesAccessedAsFirstPartyDueToUserInteraction",
        topNumberOfTimesAccessedAsFirstPartyDueToUserInteraction, significantFiguresForLoggedValues, ShouldSample::No);
    store.sendDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), descriptionPreamble + "NumberOfTimesAccessedAsFirstPartyDueToStorageAccessAPI",
        topNumberOfTimesAccessedAsFirstPartyDueToStorageAccessAPI, significantFiguresForLoggedValues, ShouldSample::No);
}
    
static void submitTopLists(const Vector<PrevalentResourceTelemetry>& sortedPrevalentResources, const Vector<PrevalentResourceTelemetry>& sortedPrevalentResourcesWithoutUserInteraction, const WebResourceLoadStatisticsStore& store)
{
    submitTopList(1, sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, store);
    
    if (sortedPrevalentResourcesWithoutUserInteraction.size() < 3)
        return;
    submitTopList(3, sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, store);
    
    if (sortedPrevalentResourcesWithoutUserInteraction.size() < 10)
        return;
    submitTopList(10, sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, store);
    
    if (sortedPrevalentResourcesWithoutUserInteraction.size() < 50)
        return;
    submitTopList(50, sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, store);
    
    if (sortedPrevalentResourcesWithoutUserInteraction.size() < 100)
        return;
    submitTopList(100, sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, store);
}
    
// This function is for testing purposes.
void static notifyPages(unsigned totalPrevalentResources, unsigned totalPrevalentResourcesWithUserInteraction, unsigned top3SubframeUnderTopFrameOrigins, const WebResourceLoadStatisticsStore& store)
{
    RunLoop::main().dispatch([totalPrevalentResources, totalPrevalentResourcesWithUserInteraction, top3SubframeUnderTopFrameOrigins, store = makeRef(store)] {
        store->notifyPageStatisticsTelemetryFinished(totalPrevalentResources, totalPrevalentResourcesWithUserInteraction, top3SubframeUnderTopFrameOrigins);
    });
}
    
// This function is for testing purposes.
void static notifyPages(const Vector<PrevalentResourceTelemetry>& sortedPrevalentResources, const Vector<PrevalentResourceTelemetry>& sortedPrevalentResourcesWithoutUserInteraction, unsigned totalNumberOfPrevalentResourcesWithUserInteraction, const WebResourceLoadStatisticsStore& store)
{
    WTF::Function<unsigned(const PrevalentResourceTelemetry& telemetry)> subframeUnderTopFrameOriginsGetter = [] (const PrevalentResourceTelemetry& t) {
        return t.subframeUnderTopFrameOrigins;
    };
    
    notifyPages(sortedPrevalentResources.size(), totalNumberOfPrevalentResourcesWithUserInteraction, median(sortedPrevalentResourcesWithoutUserInteraction, 0, 2, subframeUnderTopFrameOriginsGetter), store);
}
    
void WebResourceLoadStatisticsTelemetry::calculateAndSubmit(const ResourceLoadStatisticsMemoryStore& resourceLoadStatisticsStore)
{
    ASSERT(!RunLoop::isMain());
    
    auto sortedPrevalentResources = sortedPrevalentResourceTelemetry(resourceLoadStatisticsStore);
    if (notifyPagesWhenTelemetryWasCaptured && sortedPrevalentResources.isEmpty()) {
        notifyPages(0, 0, 0, resourceLoadStatisticsStore.store());
        return;
    }
    
    Vector<PrevalentResourceTelemetry> sortedPrevalentResourcesWithoutUserInteraction;
    sortedPrevalentResourcesWithoutUserInteraction.reserveInitialCapacity(sortedPrevalentResources.size());
    Vector<unsigned> prevalentResourcesDaysSinceUserInteraction;
    
    for (auto& prevalentResource : sortedPrevalentResources) {
        if (prevalentResource.hasHadUserInteraction)
            prevalentResourcesDaysSinceUserInteraction.append(prevalentResource.daysSinceUserInteraction);
        else
            sortedPrevalentResourcesWithoutUserInteraction.uncheckedAppend(prevalentResource);
    }
    
    // Dispatch on the main thread to make sure the WebPageProxy we're using doesn't go away.
    RunLoop::main().dispatch([sortedPrevalentResources = WTFMove(sortedPrevalentResources), sortedPrevalentResourcesWithoutUserInteraction = WTFMove(sortedPrevalentResourcesWithoutUserInteraction), prevalentResourcesDaysSinceUserInteraction = WTFMove(prevalentResourcesDaysSinceUserInteraction), &resourceLoadStatisticsStore] () {
        auto webPageProxy = WebPageProxy::nonEphemeralWebPageProxy();
        if (!webPageProxy) {
            if (notifyPagesWhenTelemetryWasCaptured)
                notifyPages(0, 0, 0, resourceLoadStatisticsStore.store());
            return;
        }
        
        if (notifyPagesWhenTelemetryWasCaptured) {
            notifyPages(sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, prevalentResourcesDaysSinceUserInteraction.size(), resourceLoadStatisticsStore.store());
            // The notify pages function is for testing so we don't need to do an actual submission.
            return;
        }
        
        webPageProxy->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), "totalNumberOfPrevalentResources"_s, sortedPrevalentResources.size(), significantFiguresForLoggedValues, ShouldSample::No);
        webPageProxy->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), "totalNumberOfPrevalentResourcesWithUserInteraction"_s, prevalentResourcesDaysSinceUserInteraction.size(), significantFiguresForLoggedValues, ShouldSample::No);
        
        if (prevalentResourcesDaysSinceUserInteraction.size() > 0)
            webPageProxy->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), "topPrevalentResourceWithUserInteractionDaysSinceUserInteraction"_s, prevalentResourcesDaysSinceUserInteraction[0], significantFiguresForLoggedValues, ShouldSample::No);
        if (prevalentResourcesDaysSinceUserInteraction.size() > 1)
            webPageProxy->logDiagnosticMessageWithValue(DiagnosticLoggingKeys::resourceLoadStatisticsTelemetryKey(), "medianPrevalentResourcesWithUserInteractionDaysSinceUserInteraction"_s, median(prevalentResourcesDaysSinceUserInteraction), significantFiguresForLoggedValues, ShouldSample::No);
        
        submitTopLists(sortedPrevalentResources, sortedPrevalentResourcesWithoutUserInteraction, resourceLoadStatisticsStore.store());
    });
}
    
void WebResourceLoadStatisticsTelemetry::setNotifyPagesWhenTelemetryWasCaptured(bool always)
{
    notifyPagesWhenTelemetryWasCaptured = always;
}
    
}

#endif
