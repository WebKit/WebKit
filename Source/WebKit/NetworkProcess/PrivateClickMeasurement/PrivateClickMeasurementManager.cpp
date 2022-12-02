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
#include "PrivateClickMeasurementDebugInfo.h"
#include "PrivateClickMeasurementNetworkLoader.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FetchOptions.h>
#include <WebCore/FormData.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/RuntimeApplicationChecks.h>
#include <pal/crypto/CryptoDigest.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

using SourceSite = WebCore::PCM::SourceSite;
using AttributionDestinationSite = WebCore::PCM::AttributionDestinationSite;
using AttributionTriggerData = WebCore::PCM::AttributionTriggerData;
using EphemeralNonce = WebCore::PCM::EphemeralNonce;

constexpr Seconds debugModeSecondsUntilSend { 10_s };

PrivateClickMeasurementManager::PrivateClickMeasurementManager(UniqueRef<PCM::Client>&& client, const String& storageDirectory)
    : m_firePendingAttributionRequestsTimer(RunLoop::main(), this, &PrivateClickMeasurementManager::firePendingAttributionRequests)
    , m_storageDirectory(storageDirectory)
    , m_client(WTFMove(client))
{
    // We should send any pending attributions on session-start in case their
    // send delay has expired while the session was closed. Waiting 5 seconds accounts for the
    // delay in database startup. When running in the daemon, the xpc activity does this instead.
    if (!m_client->runningInDaemon())
        startTimer(5_s);
}

PrivateClickMeasurementManager::~PrivateClickMeasurementManager()
{
    if (m_store)
        m_store->close([] { });
}

void PrivateClickMeasurementManager::storeUnattributed(PrivateClickMeasurement&& measurement, CompletionHandler<void()>&& completionHandler)
{
    if (!featureEnabled())
        return completionHandler();

    clearExpired();

    if (m_privateClickMeasurementAppBundleIDForTesting)
        measurement.setSourceApplicationBundleIDForTesting(*m_privateClickMeasurementAppBundleIDForTesting);

    if (measurement.ephemeralSourceNonce()) {
        auto measurementCopy = measurement;
        // This is guaranteed to be close in time to the navigational click which makes it likely to be personally identifiable.
        getTokenPublicKey(WTFMove(measurementCopy), WebCore::PCM::AttributionReportEndpoint::Source, PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable, [weakThis = WeakPtr { *this }, this] (PrivateClickMeasurement&& measurement, const String& publicKeyBase64URL) {
            if (!weakThis)
                return;

            if (publicKeyBase64URL.isEmpty())
                return;

            if (m_fraudPreventionValuesForTesting)
                measurement.setSourceUnlinkableTokenValue(m_fraudPreventionValuesForTesting->unlinkableTokenForSource);
#if PLATFORM(COCOA)
            else {
                if (auto errorMessage = measurement.calculateAndUpdateSourceUnlinkableToken(publicKeyBase64URL)) {
                    RELEASE_LOG_INFO(PrivateClickMeasurement, "Got the following error in calculateAndUpdateSourceUnlinkableToken(): '%{public}s", errorMessage->utf8().data());
                    m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] "_s, *errorMessage));
                    return;
                }
            }
#endif

            getSignedUnlinkableTokenForSource(WTFMove(measurement));
            return;
        });
    }

    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] Storing a click."_s);

    insertPrivateClickMeasurement(WTFMove(measurement), PrivateClickMeasurementAttributionType::Unattributed, WTFMove(completionHandler));
}

void PrivateClickMeasurementManager::getTokenPublicKey(PrivateClickMeasurement&& attribution, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried pcmDataCarried, Function<void(PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL)>&& callback)
{
    if (!featureEnabled())
        return;

    URL tokenPublicKeyURL;
    if (m_tokenPublicKeyURLForTesting) {
        if (attributionReportEndpoint == WebCore::PCM::AttributionReportEndpoint::Destination)
            return;
        tokenPublicKeyURL = *m_tokenPublicKeyURLForTesting;
        // FIXME(225364)
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;
    } else {
        if (auto optURL = attribution.tokenPublicKeyURL())
            tokenPublicKeyURL = *optURL;
        else
            return;
    }

    if (tokenPublicKeyURL.isEmpty() || !tokenPublicKeyURL.isValid())
        return;

    if (debugModeEnabled())
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a token public key request.");
    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] About to fire a token public key request."_s);

    PCM::NetworkLoader::start(WTFMove(tokenPublicKeyURL), nullptr, pcmDataCarried, [weakThis = WeakPtr { *this }, this, attribution = WTFMove(attribution), callback = WTFMove(callback)] (auto& errorDescription, auto& jsonObject) mutable {
        if (!weakThis)
            return;

        if (!errorDescription.isNull()) {
            m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, errorDescription, "' for token public key request."_s));
            return;
        }

        if (!jsonObject) {
            m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] JSON response is empty for token public key request."_s));
            return;
        }

        m_client->broadcastConsoleMessage(MessageLevel::Log, makeString("[Private Click Measurement] Got JSON response for token public key request."_s));

        callback(WTFMove(attribution), jsonObject->getString("token_public_key"_s));
    });
}

void PrivateClickMeasurementManager::getTokenPublicKey(AttributionTriggerData&& attributionTriggerData, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried pcmDataCarried, Function<void(AttributionTriggerData&& attributionTriggerData, const String& publicKeyBase64URL)>&& callback)
{
    if (!featureEnabled())
        return;

    URL tokenPublicKeyURL;
    if (m_tokenPublicKeyURLForTesting) {
        if (attributionReportEndpoint == WebCore::PCM::AttributionReportEndpoint::Source)
            return;
        tokenPublicKeyURL = *m_tokenPublicKeyURLForTesting;
        // FIXME(225364)
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;
    } else {
        if (auto optURL = attributionTriggerData.tokenPublicKeyURL())
            tokenPublicKeyURL = *optURL;
        else
            return;
    }

    if (tokenPublicKeyURL.isEmpty() || !tokenPublicKeyURL.isValid())
        return;

    if (debugModeEnabled())
        pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a token public key request.");
    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] About to fire a token public key request."_s);

    PCM::NetworkLoader::start(WTFMove(tokenPublicKeyURL), nullptr, pcmDataCarried, [weakThis = WeakPtr { *this }, this, attributionTriggerData = WTFMove(attributionTriggerData), callback = WTFMove(callback)] (auto& errorDescription, auto& jsonObject) mutable {
        if (!weakThis)
            return;

        if (!errorDescription.isNull()) {
            m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, errorDescription, "' for token public key request."_s));
            return;
        }

        if (!jsonObject) {
            m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] JSON response is empty for token public key request."_s));
            return;
        }

        m_client->broadcastConsoleMessage(MessageLevel::Log, makeString("[Private Click Measurement] Got JSON response for token public key request."_s));

        callback(WTFMove(attributionTriggerData), jsonObject->getString("token_public_key"_s));
    });
}

void PrivateClickMeasurementManager::configureForTokenSigning(PrivateClickMeasurement::PcmDataCarried& pcmDataCarried, URL& tokenSignatureURL, std::optional<URL> givenTokenSignatureURL)
{
    // This is guaranteed to be close in time to a page load which can identify the user, either the source or the destination.
    pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable;
    if (m_tokenSignatureURLForTesting) {
        tokenSignatureURL = *m_tokenSignatureURLForTesting;
        // FIXME(225364)
        if (!debugModeEnabled())
            pcmDataCarried = PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;
    } else if (givenTokenSignatureURL)
        tokenSignatureURL = *givenTokenSignatureURL;
}

std::optional<String> PrivateClickMeasurementManager::getSignatureBase64URLFromTokenSignatureResponse(const String& errorDescription, const RefPtr<JSON::Object>& jsonObject)
{
    if (!errorDescription.isNull()) {
        m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, errorDescription, "' for token signing request."_s));
        return std::nullopt;
    }

    if (!jsonObject) {
        m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] JSON response is empty for token signing request."_s));
        return std::nullopt;
    }

    auto signatureBase64URL = jsonObject->getString("unlinkable_token"_s);
    if (signatureBase64URL.isEmpty()) {
        m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] JSON response doesn't have the key 'unlinkable_token' for token signing request."_s));
        return std::nullopt;
    }

    return signatureBase64URL;
}

void PrivateClickMeasurementManager::getSignedUnlinkableTokenForSource(PrivateClickMeasurement&& measurement)
{
    if (!featureEnabled())
        return;

    PrivateClickMeasurement::PcmDataCarried pcmDataCarried;
    URL tokenSignatureURL;
    configureForTokenSigning(pcmDataCarried, tokenSignatureURL, measurement.tokenSignatureURL());

    if (tokenSignatureURL.isEmpty() || !tokenSignatureURL.isValid())
        return;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a unlinkable token signing request for the click source.");
    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] About to fire a unlinkable token signing request for the click source."_s);

    PCM::NetworkLoader::start(WTFMove(tokenSignatureURL), measurement.tokenSignatureJSON(), pcmDataCarried, [weakThis = WeakPtr { *this }, this, measurement = WTFMove(measurement)] (auto& errorDescription, auto& jsonObject) mutable {
        if (!weakThis)
            return;

        auto signatureBase64URL = getSignatureBase64URLFromTokenSignatureResponse(errorDescription, jsonObject);
        if (!signatureBase64URL)
            return;

        if (m_fraudPreventionValuesForTesting) {
            WebCore::PCM::SourceSecretToken sourceSecretToken;
            sourceSecretToken.tokenBase64URL = m_fraudPreventionValuesForTesting->secretTokenForSource;
            sourceSecretToken.signatureBase64URL = m_fraudPreventionValuesForTesting->signatureForSource;
            sourceSecretToken.keyIDBase64URL = m_fraudPreventionValuesForTesting->keyIDForSource;
            measurement.setSourceSecretToken(WTFMove(sourceSecretToken));
#if PLATFORM(COCOA)
        } else {
            if (auto errorMessage = measurement.calculateAndUpdateSourceSecretToken(*signatureBase64URL)) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Got the following error in calculateAndUpdateSourceSecretToken(): '%{public}s", errorMessage->utf8().data());
                m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] "_s, *errorMessage));
                return;
            }
#endif
        }

        m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] Storing a secret token."_s);

        insertPrivateClickMeasurement(WTFMove(measurement), PrivateClickMeasurementAttributionType::Unattributed, [] { });
    });

}

void PrivateClickMeasurementManager::getSignedUnlinkableTokenForDestination(SourceSite&& sourceSite, AttributionDestinationSite&& destinationSite, AttributionTriggerData&& attributionTriggerData, const ApplicationBundleIdentifier& applicationBundleIdentifier)
{
    if (!featureEnabled())
        return;

    PrivateClickMeasurement::PcmDataCarried pcmDataCarried;
    URL tokenSignatureURL;
    configureForTokenSigning(pcmDataCarried, tokenSignatureURL, attributionTriggerData.tokenSignatureURL());

    if (tokenSignatureURL.isEmpty() || !tokenSignatureURL.isValid())
        return;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire a unlinkable token signing request for the click destination.");
    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] About to fire a unlinkable token signing request for the click destination."_s);

    PCM::NetworkLoader::start(WTFMove(tokenSignatureURL), attributionTriggerData.tokenSignatureJSON(), pcmDataCarried, [weakThis = WeakPtr { *this }, this, sourceSite = WTFMove(sourceSite), destinationSite = WTFMove(destinationSite), attributionTriggerData = WTFMove(attributionTriggerData), applicationBundleIdentifier = applicationBundleIdentifier.isolatedCopy()] (auto& errorDescription, auto& jsonObject) mutable {
        if (!weakThis)
            return;

        auto signatureBase64URL = getSignatureBase64URLFromTokenSignatureResponse(errorDescription, jsonObject);
        if (!signatureBase64URL)
            return;

#if PLATFORM(COCOA)
            if (!attributionTriggerData.destinationUnlinkableToken) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Destination unlinkable token is missing.");
                m_client->broadcastConsoleMessage(MessageLevel::Error, "[Private Click Measurement] Destination unlinkable token is missing."_s);
                return;
            }

            auto result = PrivateClickMeasurement::calculateAndUpdateDestinationSecretToken(*signatureBase64URL, *attributionTriggerData.destinationUnlinkableToken);
            if (!result) {
                auto errorMessage = result.error().isEmpty() ? "Unknown"_s : result.error();
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Got the following error in calculateAndUpdateSourceSecretToken(): '%{public}s", errorMessage.utf8().data());
                m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] "_s, errorMessage));
                return;
            }

            m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] Storing a secret token."_s);

            attributionTriggerData.destinationSecretToken = result.value();
            attribute(WTFMove(sourceSite), WTFMove(destinationSite), WTFMove(attributionTriggerData), m_privateClickMeasurementAppBundleIDForTesting ? *m_privateClickMeasurementAppBundleIDForTesting : applicationBundleIdentifier);
#endif
    });
}

void PrivateClickMeasurementManager::insertPrivateClickMeasurement(PrivateClickMeasurement&& measurement, PrivateClickMeasurementAttributionType type, CompletionHandler<void()>&& completionHandler)
{
    store().insertPrivateClickMeasurement(WTFMove(measurement), type, WTFMove(completionHandler));
}

void PrivateClickMeasurementManager::migratePrivateClickMeasurementFromLegacyStorage(PrivateClickMeasurement&& measurement, PrivateClickMeasurementAttributionType type)
{
    store().insertPrivateClickMeasurement(WTFMove(measurement), type, [] { });
}

void PrivateClickMeasurementManager::setDebugModeIsEnabled(bool enabled)
{
    // This doesn't maintain global state, it just broadcasts a message when debug mode enabled changes.
    // The state is either stored in NetworkSession when not using the daemon
    // or in DaemonConnectionSet per-connection when using the daemon.

    auto message = enabled ? "[Private Click Measurement] Turned Debug Mode on."_s : "[Private Click Measurement] Turned Debug Mode off."_s;
    m_client->broadcastConsoleMessage(MessageLevel::Info, message);
}

void PrivateClickMeasurementManager::handleAttribution(AttributionTriggerData&& attributionTriggerData, const URL& requestURL, WebCore::RegistrableDomain&& redirectDomain, const URL& firstPartyURL, const ApplicationBundleIdentifier& applicationBundleIdentifier)
{
    if (!featureEnabled())
        return;

    if (!redirectDomain.matches(requestURL)) {
        m_client->broadcastConsoleMessage(MessageLevel::Warning, "[Private Click Measurement] Triggering event was not accepted because the HTTP redirect was not same-site."_s);
        return;
    }

    RegistrableDomain sourceDomain;
    if (redirectDomain.matches(firstPartyURL)) {
        if (!attributionTriggerData.sourceRegistrableDomain) {
            m_client->broadcastConsoleMessage(MessageLevel::Warning, "[Private Click Measurement] Triggering event was not accepted because it was requested in an HTTP redirect that is same-site as the first-party and no attributionSource query parameter was provided."_s);
            return;
        }
        sourceDomain = *attributionTriggerData.sourceRegistrableDomain;
    } else if (attributionTriggerData.sourceRegistrableDomain) {
        m_client->broadcastConsoleMessage(MessageLevel::Warning, "[Private Click Measurement] Triggering event was not accepted because it was requested in an HTTP redirect that is cross-site from the first-party but an attributionSource query parameter was still provided."_s);
        return;
    } else
        sourceDomain = WTFMove(redirectDomain);

    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] Triggering event accepted."_s);

    // Signing of a destination token must be done unconditionally. Otherwise it becomes a way to discover that there is a pending attribution.
    if (attributionTriggerData.ephemeralDestinationNonce) {
        auto attributionTriggerDataCopy = attributionTriggerData;
        // This is guaranteed to be close in time to the triggering event which makes it likely to be personally identifiable.
        getTokenPublicKey(WTFMove(attributionTriggerDataCopy), WebCore::PCM::AttributionReportEndpoint::Destination, PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable, [weakThis = WeakPtr { *this }, this, sourceSite = SourceSite { WTFMove(sourceDomain) }, destinationSite = AttributionDestinationSite { firstPartyURL }, applicationBundleIdentifier = applicationBundleIdentifier.isolatedCopy()] (AttributionTriggerData&& attributionTriggerData, const String& publicKeyBase64URL) mutable {
            if (!weakThis)
                return;

            if (publicKeyBase64URL.isEmpty()) {
                RELEASE_LOG_INFO(PrivateClickMeasurement, "The public key URL was empty.");
                m_client->broadcastConsoleMessage(MessageLevel::Error, "[Private Click Measurement] The public key URL was empty."_s);
                return;
            }

            if (m_fraudPreventionValuesForTesting)
                attributionTriggerData.setDestinationUnlinkableTokenValue(m_fraudPreventionValuesForTesting->unlinkableTokenForDestination);
#if PLATFORM(COCOA)
            else {
                auto result = PrivateClickMeasurement::calculateAndUpdateDestinationUnlinkableToken(publicKeyBase64URL);
                if (result) {
                    attributionTriggerData.destinationUnlinkableToken = result.value();
                    getSignedUnlinkableTokenForDestination(WTFMove(sourceSite), WTFMove(destinationSite), WTFMove(attributionTriggerData), m_privateClickMeasurementAppBundleIDForTesting ? *m_privateClickMeasurementAppBundleIDForTesting : applicationBundleIdentifier);
                    return;
                }

                auto errorMessage = result.error().isEmpty() ? "Unknown"_s : result.error();
                RELEASE_LOG_INFO(PrivateClickMeasurement, "Got the following error in calculateAndUpdateDestinationUnlinkableToken(): '%{public}s", errorMessage.utf8().data());
                m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] "_s, errorMessage));
                return;
            }
#endif
        });
        return;
    }

    attribute(SourceSite { WTFMove(sourceDomain) }, AttributionDestinationSite { firstPartyURL }, WTFMove(attributionTriggerData), m_privateClickMeasurementAppBundleIDForTesting ? *m_privateClickMeasurementAppBundleIDForTesting : applicationBundleIdentifier);
}

void PrivateClickMeasurementManager::startTimerImmediatelyForTesting()
{
    startTimer(0_s);
}

void PrivateClickMeasurementManager::setPrivateClickMeasurementAppBundleIDForTesting(ApplicationBundleIdentifier&& appBundleID)
{
    if (appBundleID.isEmpty())
        m_privateClickMeasurementAppBundleIDForTesting = std::nullopt;
    else
        m_privateClickMeasurementAppBundleIDForTesting = WTFMove(appBundleID);
}

void PrivateClickMeasurementManager::startTimer(Seconds seconds)
{
    m_firePendingAttributionRequestsTimer.startOneShot(seconds);
}

void PrivateClickMeasurementManager::attribute(SourceSite&& sourceSite, AttributionDestinationSite&& destinationSite, AttributionTriggerData&& attributionTriggerData, const ApplicationBundleIdentifier& applicationBundleIdentifier)
{
    if (!featureEnabled())
        return;

    store().attributePrivateClickMeasurement(WTFMove(sourceSite), WTFMove(destinationSite), applicationBundleIdentifier, WTFMove(attributionTriggerData), m_isRunningTest ? WebCore::PrivateClickMeasurement::IsRunningLayoutTest::Yes : WebCore::PrivateClickMeasurement::IsRunningLayoutTest::No, [this, weakThis = WeakPtr { *this }] (auto attributionSecondsUntilSendData, auto debugInfo) {
        if (!weakThis)
            return;
        
        if (!attributionSecondsUntilSendData)
            return;

        if (UNLIKELY(debugModeEnabled())) {
            for (auto& message : debugInfo.messages)
                m_client->broadcastConsoleMessage(message.messageLevel, message.message);
        }

        if (attributionSecondsUntilSendData.value().hasValidSecondsUntilSendValues()) {
            auto minSecondsUntilSend = attributionSecondsUntilSendData.value().minSecondsUntilSend();

            ASSERT(minSecondsUntilSend);
            if (!minSecondsUntilSend)
                return;

            if (m_firePendingAttributionRequestsTimer.isActive() && m_firePendingAttributionRequestsTimer.secondsUntilFire() < *minSecondsUntilSend)
                return;

            if (UNLIKELY(debugModeEnabled())) {
                m_client->broadcastConsoleMessage(MessageLevel::Log, makeString("[Private Click Measurement] Setting timer for firing attribution request to the debug mode timeout of "_s, debugModeSecondsUntilSend.seconds(), " seconds where the regular timeout would have been "_s, minSecondsUntilSend.value().seconds(), " seconds."_s));
                minSecondsUntilSend = debugModeSecondsUntilSend;
            } else
                m_client->broadcastConsoleMessage(MessageLevel::Log, makeString("[Private Click Measurement] Setting timer for firing attribution request to the timeout of "_s, minSecondsUntilSend.value().seconds(), " seconds."_s));

            startTimer(*minSecondsUntilSend);
        }
    });
}

void PrivateClickMeasurementManager::fireConversionRequest(const PrivateClickMeasurement& attribution, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    if (!featureEnabled() || !attribution.attributionTriggerData())
        return;

    if (!attribution.sourceSecretToken() && !attribution.attributionTriggerData()->destinationSecretToken) {
        fireConversionRequestImpl(attribution, attributionReportEndpoint);
        return;
    }

    auto attributionCopy = attribution;
    // This happens out of webpage context and with a long delay and is thus unlikely to be personally identifiable.
    if (attribution.sourceSecretToken()) {
        getTokenPublicKey(WTFMove(attributionCopy), attributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable, [weakThis = WeakPtr { *this }, this, attributionReportEndpoint] (PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL) {
            if (!weakThis)
                return;

            auto publicKeyData = base64URLDecode(publicKeyBase64URL);
            if (!publicKeyData)
                return;

            auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
            crypto->addBytes(publicKeyData->data(), publicKeyData->size());
            auto publicKeyDataHash = crypto->computeHash();

            auto keyID = base64URLEncodeToString(publicKeyDataHash.data(), publicKeyDataHash.size());
            if (keyID != attribution.sourceSecretToken()->keyIDBase64URL)
                return;

            if (!attribution.attributionTriggerData())
                return;

            if (attribution.attributionTriggerData()->destinationSecretToken) {
                getTokenPublicKey(WTFMove(attribution), attributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable, [weakThis = WeakPtr { *this }, this, attributionReportEndpoint] (PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL) {
                    if (!weakThis)
                        return;

                    auto publicKeyData = base64URLDecode(publicKeyBase64URL);
                    if (!publicKeyData)
                        return;

                    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
                    crypto->addBytes(publicKeyData->data(), publicKeyData->size());
                    auto publicKeyDataHash = crypto->computeHash();

                    auto keyID = base64URLEncodeToString(publicKeyDataHash.data(), publicKeyDataHash.size());
                    if (keyID != attribution.attributionTriggerData()->destinationSecretToken->keyIDBase64URL)
                        return;

                    fireConversionRequestImpl(attribution, attributionReportEndpoint);
                });
                return;
            }

            fireConversionRequestImpl(attribution, attributionReportEndpoint);
        });
        return;
    }

    getTokenPublicKey(WTFMove(attributionCopy), attributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable, [weakThis = WeakPtr { *this }, this, attributionReportEndpoint] (PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL) {
        if (!weakThis)
            return;

        auto publicKeyData = base64URLDecode(publicKeyBase64URL);
        if (!publicKeyData)
            return;

        auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
        crypto->addBytes(publicKeyData->data(), publicKeyData->size());
        auto publicKeyDataHash = crypto->computeHash();

        auto keyID = base64URLEncodeToString(publicKeyDataHash.data(), publicKeyDataHash.size());
        if (!attribution.attributionTriggerData()->destinationSecretToken || keyID != attribution.attributionTriggerData()->destinationSecretToken->keyIDBase64URL)
            return;

        fireConversionRequestImpl(attribution, attributionReportEndpoint);
    });
}

void PrivateClickMeasurementManager::fireConversionRequestImpl(const PrivateClickMeasurement& attribution, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    URL attributionURL;
    switch (attributionReportEndpoint) {
    case WebCore::PCM::AttributionReportEndpoint::Source:
        attributionURL = m_attributionReportTestConfig ? m_attributionReportTestConfig->attributionReportClickSourceURL : attribution.attributionReportClickSourceURL();
        break;
    case WebCore::PCM::AttributionReportEndpoint::Destination:
        attributionURL = m_attributionReportTestConfig ? m_attributionReportTestConfig->attributionReportClickDestinationURL : attribution.attributionReportClickDestinationURL();
    }

    if (attributionURL.isEmpty() || !attributionURL.isValid())
        return;

    auto pcmDataCarried = debugModeEnabled() ? PrivateClickMeasurement::PcmDataCarried::PersonallyIdentifiable : PrivateClickMeasurement::PcmDataCarried::NonPersonallyIdentifiable;

    RELEASE_LOG_INFO(PrivateClickMeasurement, "About to fire an attribution request.");
    m_client->broadcastConsoleMessage(MessageLevel::Log, "[Private Click Measurement] About to fire an attribution request."_s);

    PCM::NetworkLoader::start(WTFMove(attributionURL), attribution.attributionReportJSON(), pcmDataCarried, [weakThis = WeakPtr { *this }, this](auto& errorDescription, auto&) {
        if (!weakThis)
            return;

        if (!errorDescription.isNull())
            m_client->broadcastConsoleMessage(MessageLevel::Error, makeString("[Private Click Measurement] Received error: '"_s, errorDescription, "' for ad click attribution request."_s));
    });
}

void PrivateClickMeasurementManager::clearSentAttribution(PrivateClickMeasurement&& sentConversion, WebCore::PCM::AttributionReportEndpoint attributionReportEndpoint)
{
    if (!featureEnabled())
        return;

    store().clearSentAttribution(WTFMove(sentConversion), attributionReportEndpoint);
}

Seconds PrivateClickMeasurementManager::randomlyBetweenFifteenAndThirtyMinutes() const
{
    if (m_isRunningTest)
        return 0_s;

    return debugModeEnabled() ? debugModeSecondsUntilSend : 15_min + Seconds(cryptographicallyRandomNumber<uint32_t>() % 900);
}

void PrivateClickMeasurementManager::firePendingAttributionRequests()
{
    if (!featureEnabled())
        return;

    store().allAttributedPrivateClickMeasurement([this, weakThis = WeakPtr { *this }] (auto&& attributions) {
        if (!weakThis)
            return;
        auto nextTimeToFire = Seconds::infinity();
        bool hasSentAttribution = false;

        for (auto& attribution : attributions) {
            std::optional<WallTime> earliestTimeToSend = attribution.timesToSend().earliestTimeToSend();
            std::optional<WebCore::PCM::AttributionReportEndpoint> attributionReportEndpoint = attribution.timesToSend().attributionReportEndpoint();

            if (!earliestTimeToSend || !attributionReportEndpoint) {
                ASSERT_NOT_REACHED();
                continue;
            }

            auto now = WallTime::now();
            if (*earliestTimeToSend <= now || m_isRunningTest || debugModeEnabled()) {
                if (hasSentAttribution) {
                    // We've already sent an attribution this round. We should send additional overdue attributions at
                    // a random time between 15 and 30 minutes to avoid a burst of simultaneous attributions. If debug
                    // mode is enabled, this should be much shorter for easy testing.
                    startTimer(randomlyBetweenFifteenAndThirtyMinutes());
                    return;
                }

                auto laterTimeToSend = attribution.timesToSend().latestTimeToSend();
                fireConversionRequest(attribution, *attributionReportEndpoint);
                clearSentAttribution(WTFMove(attribution), *attributionReportEndpoint);
                hasSentAttribution = true;

                // Update nextTimeToFire in case the later report time for this attribution is sooner than the scheduled next time to fire.
                // Or, if debug mode is enabled, we should send the second report on a much shorter delay for easy testing.
                if (laterTimeToSend) {
                    Seconds laterTimeToSendInSecondsFromNow = (*laterTimeToSend - WallTime::now());
                    // Avoid sending expired attributions in bursts by using a random 15-30 minute interval if laterTimeToSend is expired.
                    laterTimeToSendInSecondsFromNow = laterTimeToSendInSecondsFromNow.value() < 0 ? randomlyBetweenFifteenAndThirtyMinutes() : laterTimeToSendInSecondsFromNow;
                    nextTimeToFire = debugModeEnabled() ? debugModeSecondsUntilSend : std::min(nextTimeToFire, laterTimeToSendInSecondsFromNow);
                }
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
}

void PrivateClickMeasurementManager::clear(CompletionHandler<void()>&& completionHandler)
{
    m_firePendingAttributionRequestsTimer.stop();
    m_privateClickMeasurementAppBundleIDForTesting = std::nullopt;

    if (!featureEnabled())
        return completionHandler();

    store().clearPrivateClickMeasurement(WTFMove(completionHandler));
}

void PrivateClickMeasurementManager::clearForRegistrableDomain(RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    if (!featureEnabled())
        return completionHandler();

    store().clearPrivateClickMeasurementForRegistrableDomain(WTFMove(domain), WTFMove(completionHandler));
}

void PrivateClickMeasurementManager::clearExpired()
{
    if (!featureEnabled())
        return;

    store().clearExpiredPrivateClickMeasurement();
}

void PrivateClickMeasurementManager::toStringForTesting(CompletionHandler<void(String)>&& completionHandler) const
{
    if (!featureEnabled())
        return completionHandler("\nNo stored Private Click Measurement data.\n"_s);

    store().privateClickMeasurementToStringForTesting(WTFMove(completionHandler));
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

void PrivateClickMeasurementManager::setAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL)
{
    if (sourceURL.isEmpty() || destinationURL.isEmpty())
        m_attributionReportTestConfig = std::nullopt;
    else
        m_attributionReportTestConfig = AttributionReportTestConfig { WTFMove(sourceURL), WTFMove(destinationURL) };
}

void PrivateClickMeasurementManager::markAllUnattributedAsExpiredForTesting()
{
    if (!featureEnabled())
        return;

    store().markAllUnattributedPrivateClickMeasurementAsExpiredForTesting();
}

void PrivateClickMeasurementManager::setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID)
{
    if (unlinkableToken.isEmpty() || secretToken.isEmpty() || signature.isEmpty() || keyID.isEmpty())
        return;
    m_fraudPreventionValuesForTesting = TestingFraudPreventionValues { WTFMove(unlinkableToken), WTFMove(secretToken), WTFMove(signature), WTFMove(keyID), emptyString(), emptyString(), emptyString(), emptyString() };
}

bool PrivateClickMeasurementManager::featureEnabled() const
{
    return m_client->featureEnabled();
}

bool PrivateClickMeasurementManager::debugModeEnabled() const
{
    return m_client->debugModeEnabled();
}

void PrivateClickMeasurementManager::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    if (!featureEnabled())
        return completionHandler();

    store().markAttributedPrivateClickMeasurementsAsExpiredForTesting(WTFMove(completionHandler));
}

PCM::Store& PrivateClickMeasurementManager::store()
{
    if (!m_store)
        m_store = PCM::Store::create(m_storageDirectory);
    return *m_store;
}

const PCM::Store& PrivateClickMeasurementManager::store() const
{
    if (!m_store)
        m_store = PCM::Store::create(m_storageDirectory);
    return *m_store;
}

void PrivateClickMeasurementManager::destroyStoreForTesting(CompletionHandler<void()>&& completionHandler)
{
    if (!m_store)
        return completionHandler();
    m_store->close([weakThis = WeakPtr { *this }, completionHandler = WTFMove(completionHandler)] () mutable {
        if (weakThis)
            weakThis->m_store = nullptr;
        return completionHandler();
    });
}

void PrivateClickMeasurementManager::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificateInfo)
{
    PCM::NetworkLoader::allowTLSCertificateChainForLocalPCMTesting(certificateInfo);
}

} // namespace WebKit
