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
#include "AdClickAttributionManager.h"

#include "Logging.h"
#include <WebCore/FetchOptions.h>
#include <WebCore/FormData.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <WebCore/RuntimeEnabledFeatures.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

using Source = AdClickAttribution::Source;
using Destination = AdClickAttribution::Destination;
using DestinationMap = HashMap<Destination, AdClickAttribution>;
using Conversion = AdClickAttribution::Conversion;

constexpr Seconds debugModeSecondsUntilSend { 60_s };

void AdClickAttributionManager::storeUnconverted(AdClickAttribution&& attribution)
{
    clearExpired();

    RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Storing an ad click.");
    m_unconvertedAdClickAttributionMap.set(std::make_pair(attribution.source(), attribution.destination()), WTFMove(attribution));
}

void AdClickAttributionManager::handleConversion(Conversion&& conversion, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest)
{
    if (m_sessionID.isEphemeral())
        return;

    RegistrableDomain redirectDomain { redirectRequest.url() };
    auto& firstPartyURL = redirectRequest.firstPartyForCookies();

    if (!redirectDomain.matches(requestURL)) {
        RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Conversion was not accepted because the HTTP redirect was not same-site.");
        return;
    }

    if (redirectDomain.matches(firstPartyURL)) {
        RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Conversion was not accepted because it was requested in an HTTP redirect that is same-site as the first-party.");
        return;
    }

    convert(AdClickAttribution::Source { WTFMove(redirectDomain) }, AdClickAttribution::Destination { firstPartyURL }, WTFMove(conversion));
}

void AdClickAttributionManager::startTimer(Seconds seconds)
{
    m_firePendingConversionRequestsTimer.startOneShot(m_isRunningTest ? 0_s : seconds);
}

void AdClickAttributionManager::convert(const Source& source, const Destination& destination, Conversion&& conversion)
{
    clearExpired();

    if (!conversion.isValid()) {
        RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Got an invalid conversion.");
        return;
    }

#if !RELEASE_LOG_DISABLED
    auto conversionData = conversion.data;
    auto conversionPriority = conversion.priority;
#endif

    RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Got a conversion with conversion data: %{public}u and priority: %{public}u.", conversionData, conversionPriority);

    auto secondsUntilSend = Seconds::infinity();

    auto pair = std::make_pair(source, destination);
    auto previouslyUnconvertedAttribution = m_unconvertedAdClickAttributionMap.take(pair);
    auto previouslyConvertedAttributionIter = m_convertedAdClickAttributionMap.find(pair);

    if (!previouslyUnconvertedAttribution.isEmpty()) {
        // Always convert the pending attribution and remove it from the unconverted map.
        if (auto optionalSecondsUntilSend = previouslyUnconvertedAttribution.convertAndGetEarliestTimeToSend(WTFMove(conversion))) {
            secondsUntilSend = *optionalSecondsUntilSend;
            ASSERT(secondsUntilSend != Seconds::infinity());
            RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Converted a stored ad click with conversion data: %{public}u and priority: %{public}u.", conversionData, conversionPriority);
        }

        if (previouslyConvertedAttributionIter == m_convertedAdClickAttributionMap.end())
            m_convertedAdClickAttributionMap.add(pair, WTFMove(previouslyUnconvertedAttribution));
        else if (previouslyUnconvertedAttribution.hasHigherPriorityThan(previouslyConvertedAttributionIter->value)) {
            // If the newly converted attribution has higher priority, replace the old one.
            m_convertedAdClickAttributionMap.set(pair, WTFMove(previouslyUnconvertedAttribution));
            RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Replaced a previously converted ad click with a new one with conversion data: %{public}u and priority: %{public}u because it had higher priority.", conversionData, conversionPriority);
        }
    } else if (previouslyConvertedAttributionIter != m_convertedAdClickAttributionMap.end()) {
        // If we have no newly converted attribution, re-convert the old one to respect the new priority.
        if (auto optionalSecondsUntilSend = previouslyConvertedAttributionIter->value.convertAndGetEarliestTimeToSend(WTFMove(conversion))) {
            secondsUntilSend = *optionalSecondsUntilSend;
            ASSERT(secondsUntilSend != Seconds::infinity());
            RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "Re-converted an ad click with a new one with conversion data: %{public}u and priority: %{public}u because it had higher priority.", conversionData, conversionPriority);
        }
    }

    if (secondsUntilSend == Seconds::infinity())
        return;
    
    if (m_firePendingConversionRequestsTimer.isActive() && m_firePendingConversionRequestsTimer.nextFireInterval() < secondsUntilSend)
        return;

    if (debugModeEnabled()) {
        RELEASE_LOG_INFO(AdClickAttribution, "Setting timer for firing conversion requests to the debug mode timeout of %{public}f seconds where the regular timeout would have been %{public}f seconds.", debugModeSecondsUntilSend.seconds(), secondsUntilSend.seconds());
        secondsUntilSend = debugModeSecondsUntilSend;
    }

    startTimer(secondsUntilSend);
}

void AdClickAttributionManager::fireConversionRequest(const AdClickAttribution& attribution)
{
    auto conversionURL = m_conversionBaseURLForTesting ? attribution.urlForTesting(*m_conversionBaseURLForTesting) : attribution.url();
    if (conversionURL.isEmpty() || !conversionURL.isValid())
        return;

    auto conversionReferrerURL = attribution.referrer();
    if (conversionReferrerURL.isEmpty() || !conversionReferrerURL.isValid())
        return;

    ResourceRequest request { conversionURL };
    
    request.setHTTPMethod("POST"_s);
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, "max-age=0"_s);
    request.setHTTPReferrer(conversionReferrerURL.string());

    FetchOptions options;
    options.credentials = FetchOptions::Credentials::Omit;
    options.redirect = FetchOptions::Redirect::Error;

    static uint64_t identifier = 0;
    
    NetworkResourceLoadParameters loadParameters { PAL::SessionID::defaultSessionID() };
    loadParameters.identifier = ++identifier;
    loadParameters.request = request;
    loadParameters.sourceOrigin = SecurityOrigin::create(conversionReferrerURL);
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.storedCredentialsPolicy = StoredCredentialsPolicy::EphemeralStatelessCookieless;
    loadParameters.options = options;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = true;
    loadParameters.shouldRestrictHTTPResponseAccess = false;

#if ENABLE(CONTENT_EXTENSIONS)
    loadParameters.mainDocumentURL = WTFMove(conversionReferrerURL);
#endif

    RELEASE_LOG_INFO_IF(debugModeEnabled(), AdClickAttribution, "About to fire an attribution request for a conversion.");

    m_pingLoadFunction(WTFMove(loadParameters), [](const WebCore::ResourceError& error, const WebCore::ResourceResponse& response) {
#if PLATFORM(COCOA)
        RELEASE_LOG_ERROR_IF(!error.isNull(), AdClickAttribution, "Received error: '%{public}s' for ad click attribution request.", error.localizedDescription().utf8().data());
#else
        RELEASE_LOG_ERROR_IF(!error.isNull(), AdClickAttribution, "Received error: '%s' for ad click attribution request.", error.localizedDescription().utf8().data());
#endif
        UNUSED_PARAM(response);
        UNUSED_PARAM(error);
    });
}

void AdClickAttributionManager::firePendingConversionRequests()
{
    auto nextTimeToFire = Seconds::infinity();
    for (auto& attribution : m_convertedAdClickAttributionMap.values()) {
        if (attribution.wasConversionSent()) {
            ASSERT_NOT_REACHED();
            continue;
        }
        auto earliestTimeToSend = attribution.earliestTimeToSend();
        if (!earliestTimeToSend) {
            ASSERT_NOT_REACHED();
            continue;
        }

        auto now = WallTime::now();
        if (*earliestTimeToSend <= now || m_isRunningTest || debugModeEnabled()) {
            fireConversionRequest(attribution);
            attribution.markConversionAsSent();
            continue;
        }

        auto seconds = *earliestTimeToSend - now;
        nextTimeToFire = std::min(nextTimeToFire, seconds);
    }

    m_convertedAdClickAttributionMap.removeIf([](auto& keyAndValue) {
        return keyAndValue.value.wasConversionSent();
    });

    if (nextTimeToFire < Seconds::infinity())
        startTimer(nextTimeToFire);
}

void AdClickAttributionManager::clear()
{
    m_firePendingConversionRequestsTimer.stop();
    m_unconvertedAdClickAttributionMap.clear();
    m_convertedAdClickAttributionMap.clear();
}

void AdClickAttributionManager::clearForRegistrableDomain(const RegistrableDomain& domain)
{
    m_unconvertedAdClickAttributionMap.removeIf([&domain](auto& keyAndValue) {
        return keyAndValue.key.first.registrableDomain == domain || keyAndValue.key.second.registrableDomain == domain;
    });

    m_convertedAdClickAttributionMap.removeIf([&domain](auto& keyAndValue) {
        return keyAndValue.key.first.registrableDomain == domain || keyAndValue.key.second.registrableDomain == domain;
    });
}

void AdClickAttributionManager::clearExpired()
{
    m_unconvertedAdClickAttributionMap.removeIf([](auto& keyAndValue) {
        return keyAndValue.value.hasExpired();
    });
}

void AdClickAttributionManager::toString(CompletionHandler<void(String)>&& completionHandler) const
{
    if (m_unconvertedAdClickAttributionMap.isEmpty() && m_convertedAdClickAttributionMap.isEmpty())
        return completionHandler("\nNo stored Ad Click Attribution data.\n"_s);

    unsigned unconvertedAttributionNumber = 0;
    StringBuilder builder;
    for (auto& attribution : m_unconvertedAdClickAttributionMap.values()) {
        if (!unconvertedAttributionNumber)
            builder.appendLiteral("Unconverted Ad Click Attributions:\n");
        else
            builder.append('\n');
        builder.appendLiteral("WebCore::AdClickAttribution ");
        builder.appendNumber(++unconvertedAttributionNumber);
        builder.append('\n');
        builder.append(attribution.toString());
}

    unsigned convertedAttributionNumber = 0;
    for (auto& attribution : m_convertedAdClickAttributionMap.values()) {
        if (unconvertedAttributionNumber)
            builder.append('\n');
        if (!convertedAttributionNumber)
            builder.appendLiteral("Converted Ad Click Attributions:\n");
        else
            builder.append('\n');
        builder.appendLiteral("WebCore::AdClickAttribution ");
        builder.appendNumber(++convertedAttributionNumber + unconvertedAttributionNumber);
        builder.append('\n');
        builder.append(attribution.toString());
    }

    completionHandler(builder.toString());
}

void AdClickAttributionManager::setConversionURLForTesting(URL&& testURL)
{
    if (testURL.isEmpty())
        m_conversionBaseURLForTesting = { };
    else
        m_conversionBaseURLForTesting = WTFMove(testURL);
}

void AdClickAttributionManager::markAllUnconvertedAsExpiredForTesting()
{
    for (auto& attribution : m_unconvertedAdClickAttributionMap.values())
        attribution.markAsExpired();
}

bool AdClickAttributionManager::debugModeEnabled() const
{
    return RuntimeEnabledFeatures::sharedFeatures().adClickAttributionDebugModeEnabled() && !m_sessionID.isEphemeral();
}

} // namespace WebKit
