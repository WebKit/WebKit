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
#include <pal/crypto/CryptoDigest.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

using SourceSite = PrivateClickMeasurement::SourceSite;
using AttributeOnSite = PrivateClickMeasurement::AttributeOnSite;
using AttributionTriggerData = PrivateClickMeasurement::AttributionTriggerData;
using EphemeralSourceNonce = PrivateClickMeasurement::EphemeralSourceNonce;

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

    if (attribution.ephemeralSourceNonce()) {
        auto attributionCopy = attribution;
        getTokenPublicKey(WTFMove(attributionCopy), [weakThis = makeWeakPtr(*this), this] (PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL) {
            if (!weakThis)
                return;

            if (publicKeyBase64URL.isEmpty())
                return;

            if (m_fraudPreventionValuesForTesting)
                attribution.setSourceSecretTokenValue(m_fraudPreventionValuesForTesting->secretToken);
#if PLATFORM(COCOA)
            else {
                if (!attribution.calculateAndUpdateSourceSecretToken(publicKeyBase64URL))
                    return;
            }
#endif

            getSignedSecretToken(WTFMove(attribution));
        });
    }

    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] Storing an ad click."_s);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->insertPrivateClickMeasurement(WTFMove(attribution), PrivateClickMeasurementAttributionType::Unattributed);
#endif
}

static NetworkResourceLoadParameters generateNetworkResourceLoadParameters(URL&& url, const String& httpMethod, RefPtr<JSON::Object>&& jsonPayload, PrivateClickMeasurement::PcmDataCarried dataTypeCarried)
{
    static uint64_t identifier = 0;
    
    ResourceRequest request { WTFMove(url) };
    request.setHTTPMethod(httpMethod);
    request.setHTTPHeaderField(HTTPHeaderName::CacheControl, WebCore::HTTPHeaderValues::maxAge0());
    if (jsonPayload) {
        request.setHTTPContentType(WebCore::HTTPHeaderValues::applicationJSONContentType());
        request.setHTTPBody(WebCore::FormData::create(jsonPayload->toJSONString().utf8().data()));
    }

    FetchOptions options;
    options.credentials = FetchOptions::Credentials::Omit;
    options.redirect = FetchOptions::Redirect::Error;

    NetworkResourceLoadParameters loadParameters;
    loadParameters.identifier = ++identifier;
    loadParameters.request = request;
    loadParameters.parentPID = presentingApplicationPID();
    loadParameters.storedCredentialsPolicy = StoredCredentialsPolicy::EphemeralStateless;
    loadParameters.options = options;
    loadParameters.shouldClearReferrerOnHTTPSToHTTPRedirect = true;
    loadParameters.shouldRestrictHTTPResponseAccess = false;
    loadParameters.pcmDataCarried = dataTypeCarried;
    
    return loadParameters;
}

static NetworkResourceLoadParameters generateNetworkResourceLoadParametersForHttpPost(URL&& url, Ref<JSON::Object>&& jsonPayload, PrivateClickMeasurement::PcmDataCarried dataTypeCarried)
{
    return generateNetworkResourceLoadParameters(WTFMove(url), "POST"_s, WTFMove(jsonPayload), dataTypeCarried);
}

static NetworkResourceLoadParameters generateNetworkResourceLoadParametersForHttpGet(URL&& url, PrivateClickMeasurement::PcmDataCarried dataTypeCarried)
{
    return generateNetworkResourceLoadParameters(WTFMove(url), "GET"_s, nullptr, dataTypeCarried);
}

void PrivateClickMeasurementManager::getTokenPublicKey(PrivateClickMeasurement&& attribution, Function<void(PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL)>&& callback)
{
    if (!featureEnabled())
        return;

    // This is guaranteed to be close in time to the navigational click which makes it likely to be personally identifiable.
    auto tokenPublicKeyURL = attribution.tokenPublicKeyURL();
    auto pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable;
    if (m_tokenPublicKeyURLForTesting) {
        tokenPublicKeyURL = *m_tokenPublicKeyURLForTesting;
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;
    }

    if (tokenPublicKeyURL.isEmpty() || !tokenPublicKeyURL.isValid())
        return;

    auto loadParameters = generateNetworkResourceLoadParametersForHttpGet(WTFMove(tokenPublicKeyURL), pcmDataCarried);

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a unlinkable token public key request.");
    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] About to fire a unlinkable token public key request."_s);

    m_pingLoadFunction(WTFMove(loadParameters), [weakThis = makeWeakPtr(*this), this, attribution = WTFMove(attribution), callback = WTFMove(callback)] (const WebCore::ResourceError& error, const WebCore::ResourceResponse& response) mutable {
        if (!weakThis)
            return;

        if (!error.isNull()) {
            m_networkProcess->broadcastConsoleMessage(weakThis->m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, error.localizedDescription(), "' for unlinkable token public key request."_s));
            return;
        }

        // FIXME(222217): Retrieve the public key from the content instead of the header.
        callback(WTFMove(attribution), response.httpHeaderFields().get("unlinkable_token_public_key"_s));
    });

}

void PrivateClickMeasurementManager::getSignedSecretToken(PrivateClickMeasurement&& attribution)
{
    if (!featureEnabled())
        return;

    // This is guaranteed to be close in time to the navigational click which makes it likely to be personally identifiable.
    auto tokenSignatureURL = attribution.tokenSignatureURL();
    auto pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable;
    if (m_tokenSignatureURLForTesting) {
        tokenSignatureURL = *m_tokenSignatureURLForTesting;
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;
    }

    if (tokenSignatureURL.isEmpty() || !tokenSignatureURL.isValid())
        return;

    auto loadParameters = generateNetworkResourceLoadParametersForHttpPost(WTFMove(tokenSignatureURL), attribution.tokenSignatureJSON(), pcmDataCarried);

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a secret token signing request.");
    m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] About to fire a secret token signing request."_s);

    m_pingLoadFunction(WTFMove(loadParameters), [weakThis = makeWeakPtr(*this), this, attribution = WTFMove(attribution)] (const WebCore::ResourceError& error, const WebCore::ResourceResponse& response) mutable {
        if (!weakThis)
            return;

        if (!error.isNull()) {
            m_networkProcess->broadcastConsoleMessage(weakThis->m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, error.localizedDescription(), "' for secret token signing request."_s));
            return;
        }

        // FIXME(222217): Retrieve the signature from the content instead of the header.
        auto signatureBase64URL = response.httpHeaderFields().get("secret_token_signature"_s);
        if (signatureBase64URL.isEmpty())
            return;

        if (m_fraudPreventionValuesForTesting)
            attribution.setSourceUnlinkableToken({ m_fraudPreventionValuesForTesting->unlinkableToken, m_fraudPreventionValuesForTesting->signature, m_fraudPreventionValuesForTesting->keyID });
#if PLATFORM(COCOA)
        else {
            if (!attribution.calculateAndUpdateSourceUnlinkableToken(signatureBase64URL))
                return;
        }
#endif

        m_networkProcess->broadcastConsoleMessage(m_sessionID, MessageSource::PrivateClickMeasurement, MessageLevel::Log, "[Private Click Measurement] Storing an unlinkable token."_s);

#if ENABLE(RESOURCE_LOAD_STATISTICS)
        if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
            resourceLoadStatistics->insertPrivateClickMeasurement(WTFMove(attribution), PrivateClickMeasurementAttributionType::Unattributed);
#endif
    });

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

    if (!attribution.sourceUnlinkableToken()) {
        fireConversionRequestImpl(attribution);
        return;
    }

    auto attributionCopy = attribution;
    getTokenPublicKey(WTFMove(attributionCopy), [weakThis = makeWeakPtr(*this), this] (PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL) {
        if (!weakThis)
            return;

        Vector<uint8_t> publicKeyData;
        WTF::base64URLDecode(publicKeyBase64URL, publicKeyData);

        auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
        crypto->addBytes(publicKeyData.data(), publicKeyData.size());
        auto publicKeyDataHash = crypto->computeHash();

        static const size_t keyIDSize = 4;
        auto keyID = WTF::base64URLEncode(publicKeyDataHash.data(), keyIDSize);
        if (keyID != attribution.sourceUnlinkableToken()->keyIDBase64URL)
            return;

        fireConversionRequestImpl(attribution);
    });
}

void PrivateClickMeasurementManager::fireConversionRequestImpl(const PrivateClickMeasurement& attribution)
{
    auto attributionURL = m_attributionReportBaseURLForTesting ? *m_attributionReportBaseURLForTesting : attribution.attributionReportURL();
    if (attributionURL.isEmpty() || !attributionURL.isValid())
        return;

    auto loadParameters = generateNetworkResourceLoadParametersForHttpPost(WTFMove(attributionURL), attribution.attributionReportJSON(), PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable);

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

void PrivateClickMeasurementManager::clearSentAttribution(PrivateClickMeasurement&& sentConversion)
{
#if ENABLE(RESOURCE_LOAD_STATISTICS)
    if (!featureEnabled())
        return;

    if (auto* resourceLoadStatistics = m_networkSession->resourceLoadStatistics())
        resourceLoadStatistics->clearSentAttribution(WTFMove(sentConversion));
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
        bool hasSentAttribution = false;

        for (auto& attribution : attributions) {
            auto earliestTimeToSend = attribution.earliestTimeToSend();
            if (!earliestTimeToSend) {
                ASSERT_NOT_REACHED();
                continue;
            }

            auto now = WallTime::now();
            if (*earliestTimeToSend <= now || m_isRunningTest || debugModeEnabled()) {
                if (hasSentAttribution) {
                    // We've already sent an attribution this round. We should send additional overdue attributions at
                    // a random time between 15 and 30 minutes to avoid a burst of simultaneous attributions. If debug
                    // mode is enabled, this should be every minute for easy testing.
                    auto interval = debugModeEnabled() ? 1_min : 15_min + Seconds(cryptographicallyRandomNumber() % 900);
                    startTimer(interval);
                    return;
                }

                fireConversionRequest(attribution);
                clearSentAttribution(WTFMove(attribution));
                hasSentAttribution = true;
                continue;
            }

            auto seconds = *earliestTimeToSend - now;
            nextTimeToFire = std::min(nextTimeToFire, seconds);

            // Attributions are sorted by earliestTimeToSend, so the first time we hit this there can be no further attributions
            // due for reporting, and nextTimeToFire is the minimum earliestTimeToSend value for any pending attribution.
            break;
        }

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

void PrivateClickMeasurementManager::setTokenPublicKeyURLForTesting(URL&& testURL)
{
    if (testURL.isEmpty())
        return;
    m_tokenPublicKeyURLForTesting = WTFMove(testURL);
}

void PrivateClickMeasurementManager::setTokenSignatureURLForTesting(URL&& testURL)
{
    if (testURL.isEmpty())
        m_tokenSignatureURLForTesting = { };
    else
        m_tokenSignatureURLForTesting = WTFMove(testURL);
}

void PrivateClickMeasurementManager::setAttributionReportURLForTesting(URL&& testURL)
{
    if (testURL.isEmpty())
        m_attributionReportBaseURLForTesting = { };
    else
        m_attributionReportBaseURLForTesting = WTFMove(testURL);
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

void PrivateClickMeasurementManager::setFraudPreventionValuesForTesting(String&& secretToken, String&& unlinkableToken, String&& signature, String&& keyID)
{
    if (secretToken.isEmpty() || unlinkableToken.isEmpty() || signature.isEmpty() || keyID.isEmpty())
        return;
    m_fraudPreventionValuesForTesting = TestingFraudPreventionValues { WTFMove(secretToken), WTFMove(unlinkableToken), WTFMove(signature), WTFMove(keyID) };
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
