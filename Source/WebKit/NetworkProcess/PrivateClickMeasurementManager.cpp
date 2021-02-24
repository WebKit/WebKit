/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "PrivateClickMeasurementManager.h"

#include "Logging.h"
#include "NetworkSession.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FormData.h>
#include <WebCore/HTTPHeaderValues.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

using SourceSite = PrivateClickMeasurement::SourceSite;
using AttributeOnSite = PrivateClickMeasurement::AttributeOnSite;
using AttributionTriggerData = PrivateClickMeasurement::AttributionTriggerData;

constexpr Seconds debugModeSecondsUntilSend { 10_s };

PrivateClickMeasurementManager::PrivateClickMeasurementManager(NetworkSession& networkSession, NetworkProcess& networkProcess, PAL::SessionID sessionID)
    : m_firePendingAttributionRequestsTimer(*this, &PrivateClickMeasurementManager::firePendingAttributionRequests)
    , m_networkSession(makeWeakPtr(networkSession))
    , m_networkProcess(networkProcess)
    , m_sessionID(sessionID)
    , m_pingLoadFunction([](NetworkResourceLoadParameters&& params, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&& completionHandler) {
        UNUSED_PARAM(params);
        completionHandler(WebCore::ResourceError(), WebCore::ResourceResponse());
    })
{
    // We should send any pending attributions on session-start in case their
    // send delay has expired while the session was closed. Waiting 5 seconds accounts for the
    // delay in database startup.
    startTimer(5_s);
}

void PrivateClickMeasurementManager::storeUnattributed(PrivateClickMeasurement&& attribution)
{
    if (!featureEnabled())
        return;

    clearExpired();

    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] Storing an ad click."_s);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->insertPrivateClickMeasurement(WTFMove(attribution), PrivateClickMeasurementAttributionType::Unattributed);
#endif
}

void PrivateClickMeasurementManager::handleAttribution(AttributionTriggerData&& attributionTriggerData, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest)
{
    if (!featureEnabled())
        return;

    RegistrableDomain redirectDomain { redirectRequest.url() };
    auto& firstPartyURL = redirectRequest.firstPartyForCookies();

    if (!redirectDomain.matches(requestURL)) {
        m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Warning, "[Private Click Measurement] Attribution was not accepted because the HTTP redirect was not same-site."_s);
        return;
    }

    if (redirectDomain.matches(firstPartyURL)) {
        m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Warning, "[Private Click Measurement] Attribution was not accepted because it was requested in an HTTP redirect that is same-site as the first-party."_s);
        return;
    }

    attribute(SourceSite { WTFMove(redirectDomain) }, AttributeOnSite { firstPartyURL }, WTFMove(attributionTriggerData));
}

void PrivateClickMeasurementManager::startTimer(Seconds seconds)
{
    m_firePendingAttributionRequestsTimer.startOneShot(m_isRunningTest ? 0_s : seconds);
}

void PrivateClickMeasurementManager::attribute(const SourceSite& sourceSite, const AttributeOnSite& attributeOnSite, AttributionTriggerData&& attributionTriggerData)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics()) {
        resourceLoadStatistics->attributePrivateClickMeasurement(sourceSite, attributeOnSite, WTFMove(attributionTriggerData), [this, weakThis = makeWeakPtr(*this)] (auto optionalSecondsUntilSend) {
            if (!weakThis)
                return;
            if (optionalSecondsUntilSend) {
                auto secondsUntilSend = *optionalSecondsUntilSend;
                if (m_firePendingAttributionRequestsTimer.isActive() && m_firePendingAttributionRequestsTimer.nextFireInterval() < secondsUntilSend)
                    return;

                if (UNLIKELY(debugModeEnabled())) {
                    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, makeString("[Private Click Measurement] Setting timer for firing attribution request to the debug mode timeout of "_s, debugModeSecondsUntilSend.seconds(), " seconds where the regular timeout would have been "_s, secondsUntilSend.seconds(), " seconds."_s));
                    secondsUntilSend = debugModeSecondsUntilSend;
                } else
                    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, makeString("[Private Click Measurement] Setting timer for firing attribution request to the timeout of "_s, secondsUntilSend.seconds(), " seconds."_s));

                startTimer(secondsUntilSend);
            }
        });
    }
#endif
}

void PrivateClickMeasurementManager::fireConversionRequest(const PrivateClickMeasurement& attribution)
{
    if (!featureEnabled())
        return;

    auto attributionURL = m_attributionBaseURLForTesting ? *m_attributionBaseURLForTesting : attribution.reportURL();
    if (attributionURL.isEmpty() || !attributionURL.isValid())
        return;

    ResourceRequest request { WTFMove(attributionURL) };
    
    request.setHTTPMethod("POST"_s);
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, WebCore::HTTPHeaderValues::maxAge0());

    request.setHTTPContentType(WebCore::HTTPHeaderValues::applicationJSONContentType());
    request.setHTTPBody(WebCore::FormData::create(attribution.json()->toJSONString().utf8().data()));

    FetchOptions options;
    options.credentials = FetchOptions::Credentials::Omit;
    options.redirect = FetchOptions::Redirect::Error;

    static uint64_t identifier = 0;
    
    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = ++identifier;
    loadParameters.request = request;
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.storedCredentialsPolicy = StoredCredentialsPolicy::EphemeralStateless;
    loadParameters.options = options;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = true;
    loadParameters.shouldRestrictHTTPResponseAccess = false;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire an attribution request.");
    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] About to fire an attribution request."_s);

    m_pingLoadFunction(WTFMove(loadParameters), [weakThis = makeWeakPtr(*this)](const WebCore::ResourceError& error, const WebCore::ResourceResponse& response) {
        if (!weakThis)
            return;

        if (!error.isNull()) {
            weakThis->m_networkProcess->broadcastConsoleMessage(weakThis->m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, error.localizedDescription(), "' for ad click attribution request."_s));
        }
    });
}

void PrivateClickMeasurementManager::clearSentAttributions(Vector<PrivateClickMeasurement>&& sentConversions)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->clearSentAttributions(WTFMove(sentConversions));
#endif
}

void PrivateClickMeasurementManager::firePendingAttributionRequests()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics();
    if (!resourceLoadStatistics)
        return;

    resourceLoadStatistics->allAttributedPrivateClickMeasurement([this, weakThis = makeWeakPtr(*this)] (auto&& attributions) {
        if (!weakThis)
            return;
        auto nextTimeToFire = Seconds::infinity();
        Vector<PrivateClickMeasurement> sentAttributions;
        
        for (auto& attribution : attributions) {
            auto earliestTimeToSend = attribution.earliestTimeToSend();
            if (!earliestTimeToSend) {
                ASSERT_NOT_REACHED();
                continue;
            }

            auto now = WallTime::now();
            if (*earliestTimeToSend <= now || m_isRunningTest || debugModeEnabled()) {
                fireConversionRequest(attribution);
                sentAttributions.append(WTFMove(attribution));
                continue;
            }

            auto seconds = *earliestTimeToSend - now;
            nextTimeToFire = std::min(nextTimeToFire, seconds);
        }
        
        clearSentAttributions(WTFMove(sentAttributions));

        if (nextTimeToFire < Seconds::infinity())
            startTimer(nextTimeToFire);
    });
#endif
}

void PrivateClickMeasurementManager::clear()
{
    m_firePendingAttributionRequestsTimer.stop();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->clearPrivateClickMeasurement();
#endif
}

void PrivateClickMeasurementManager::clearForRegistrableDomain(const RegistrableDomain& domain)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->clearPrivateClickMeasurementForRegistrableDomain(domain);
#endif
}

void PrivateClickMeasurementManager::clearExpired()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->clearExpiredPrivateClickMeasurement();
#endif
}

void PrivateClickMeasurementManager::toString(CompletionHandler<void(String)>&& completionHandler) const
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled()) {
        completionHandler("\nNo stored Private Click Measurement data.\n"_s);
        return;
    }

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics()) {
        resourceLoadStatistics->privateClickMeasurementToString(WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler("\nNo stored Private Click Measurement data.\n"_s);
}

void PrivateClickMeasurementManager::setConversionURLForTesting(URL&& testURL)
{
    if (testURL.isEmpty())
        m_attributionBaseURLForTesting = { };
    else
        m_attributionBaseURLForTesting = WTFMove(testURL);
}

void PrivateClickMeasurementManager::markAllUnattributedAsExpiredForTesting()
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
#endif
}

bool PrivateClickMeasurementManager::featureEnabled() const
{
    return m_networkSession && m_networkProcess->privateClickMeasurementEnabled() && !m_sessionID.isEphemeral();
}

bool PrivateClickMeasurementManager::debugModeEnabled() const
{
    return m_networkProcess->privateClickMeasurementDebugModeEnabled() && !m_sessionID.isEphemeral();
}

void PrivateClickMeasurementManager::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled()) {
        completionHandler();
        return;
    }

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics()) {
        resourceLoadStatistics->markAttributedPrivateClickMeasurementsAsExpiredForTesting(WTFMove(completionHandler));
        return;
    }
#endif
    completionHandler();
}

} // namespace WebKit
