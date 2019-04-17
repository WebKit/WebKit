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

#include <WebCore/FetchOptions.h>
#include <WebCore/FormData.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

using Source = AdClickAttribution::Source;
using Destination = AdClickAttribution::Destination;
using DestinationMap = HashMap<Destination, AdClickAttribution>;
using Conversion = AdClickAttribution::Conversion;

void AdClickAttributionManager::storeUnconverted(AdClickAttribution&& attribution)
{
    m_unconvertedAdClickAttributionMap.set(std::make_pair(attribution.source(), attribution.destination()), WTFMove(attribution));
}

void AdClickAttributionManager::startTimer(Seconds seconds)
{
    m_firePendingConversionRequestsTimer.startOneShot(m_isRunningTest ? 0_s : seconds);
}

void AdClickAttributionManager::convert(const Source& source, const Destination& destination, Conversion&& conversion)
{
    if (!conversion.isValid())
        return;

    auto secondsUntilSend = Seconds::infinity();

    auto pair = std::make_pair(source, destination);
    auto previouslyUnconvertedAttribution = m_unconvertedAdClickAttributionMap.take(pair);
    auto previouslyConvertedAttributionIter = m_convertedAdClickAttributionMap.find(pair);

    if (!previouslyUnconvertedAttribution.isEmpty()) {
        // Always convert the pending attribution and remove it from the unconverted map.
        if (auto optionalSecondsUntilSend = previouslyUnconvertedAttribution.convertAndGetEarliestTimeToSend(WTFMove(conversion))) {
            secondsUntilSend = *optionalSecondsUntilSend;
            ASSERT(secondsUntilSend != Seconds::infinity());
        }
        // If there is no previously converted attribution for this pair, add the new one.
        // If the newly converted attribution has higher priority, replace the old one.
        if (previouslyConvertedAttributionIter == m_convertedAdClickAttributionMap.end()
            || previouslyUnconvertedAttribution.hasHigherPriorityThan(previouslyConvertedAttributionIter->value))
            m_convertedAdClickAttributionMap.set(pair, WTFMove(previouslyUnconvertedAttribution));
    } else if (previouslyConvertedAttributionIter != m_convertedAdClickAttributionMap.end()) {
        // If we have no newly converted attribution, re-convert the old one to respect the new priority.
        if (auto optionalSecondsUntilSend = previouslyConvertedAttributionIter->value.convertAndGetEarliestTimeToSend(WTFMove(conversion))) {
            secondsUntilSend = *optionalSecondsUntilSend;
            ASSERT(secondsUntilSend != Seconds::infinity());
        }
    }

    if (secondsUntilSend == Seconds::infinity())
        return;
    
    if (m_firePendingConversionRequestsTimer.isActive() && m_firePendingConversionRequestsTimer.nextFireInterval() < secondsUntilSend)
        return;
    
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
    
    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = ++identifier;
    loadParameters.request = request;
    loadParameters.sourceOrigin = SecurityOrigin::create(conversionReferrerURL);
    loadParameters.parentPID = presentingApplicationPID();
    // FIXME: Switch to the use of an ephemeral, stateless session.
    loadParameters.sessionID = PAL::SessionID::defaultSessionID();
    loadParameters.storedCredentialsPolicy = StoredCredentialsPolicy::DoNotUse;
    loadParameters.options = options;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = true;
    loadParameters.shouldRestrictHTTPResponseAccess = false;

#if ENABLE(CONTENT_EXTENSIONS)
    loadParameters.mainDocumentURL = WTFMove(conversionReferrerURL);
#endif

    m_pingLoadFunction(WTFMove(loadParameters), [](const WebCore::ResourceError& error, const WebCore::ResourceResponse& response) {
        // FIXME: Add logging of errors to a dedicated channel.
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
        if (*earliestTimeToSend <= now || m_isRunningTest) {
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

void AdClickAttributionManager::clear(CompletionHandler<void()>&& completionHandler)
{
    m_firePendingConversionRequestsTimer.stop();
    m_unconvertedAdClickAttributionMap.clear();
    m_convertedAdClickAttributionMap.clear();
    completionHandler();
}

void AdClickAttributionManager::toString(CompletionHandler<void(String)>&& completionHandler) const
{
    if (m_unconvertedAdClickAttributionMap.isEmpty() && m_convertedAdClickAttributionMap.isEmpty())
        return completionHandler("No stored Ad Click Attribution data.\n"_s);

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

} // namespace WebKit
