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

#pragma once

#include "NetworkLoadParameters.h"
#include "NetworkProcess.h"
#include "PrivateClickMeasurementClient.h"
#include "PrivateClickMeasurementManagerInterface.h"
#include "PrivateClickMeasurementStore.h"
#include <WebCore/Timer.h>
#include <wtf/CompletionHandler.h>
#include <wtf/JSONValues.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class PrivateClickMeasurementManager : public PCM::ManagerInterface, public CanMakeWeakPtr<PrivateClickMeasurementManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:

    explicit PrivateClickMeasurementManager(UniqueRef<PCM::Client>&&, const String& storageDirectory);
    ~PrivateClickMeasurementManager();

    using ApplicationBundleIdentifier = String;

    void storeUnattributed(PrivateClickMeasurement&&, CompletionHandler<void()>&&) final;
    void handleAttribution(AttributionTriggerData&&, const URL& requestURL, WebCore::RegistrableDomain&& redirectDomain, const URL& firstPartyURL, const ApplicationBundleIdentifier&) final;
    void clear(CompletionHandler<void()>&&) final;
    void clearForRegistrableDomain(RegistrableDomain&&, CompletionHandler<void()>&&) final;
    void migratePrivateClickMeasurementFromLegacyStorage(PrivateClickMeasurement&&, PrivateClickMeasurementAttributionType) final;
    void setDebugModeIsEnabled(bool) final;
    void firePendingAttributionRequests();

    void toStringForTesting(CompletionHandler<void(String)>&&) const final;
    void setOverrideTimerForTesting(bool value) final { m_isRunningTest = value; }
    void setTokenPublicKeyURLForTesting(URL&&) final;
    void setTokenSignatureURLForTesting(URL&&) final;
    void setAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL) final;
    void markAllUnattributedAsExpiredForTesting() final;
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&) final;
    void setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID) final;
    void startTimerImmediatelyForTesting() final;
    void setPrivateClickMeasurementAppBundleIDForTesting(ApplicationBundleIdentifier&&);
    void destroyStoreForTesting(CompletionHandler<void()>&&) final;
    void allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo&) final;

private:
    PCM::Store& store();
    const PCM::Store& store() const;
    void startTimer(Seconds);
    void getTokenPublicKey(PrivateClickMeasurement&&, WebCore::PCM::AttributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried, Function<void(PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL)>&&);
    void getTokenPublicKey(AttributionTriggerData&&, WebCore::PCM::AttributionReportEndpoint, PrivateClickMeasurement::PcmDataCarried, Function<void(AttributionTriggerData&&, const String& publicKeyBase64URL)>&&);
    void configureForTokenSigning(PrivateClickMeasurement::PcmDataCarried&, URL& tokenSignatureURL, std::optional<URL> givenTokenSignatureURL);
    std::optional<String> getSignatureBase64URLFromTokenSignatureResponse(const String& errorDescription, const RefPtr<JSON::Object>&);
    void getSignedUnlinkableTokenForSource(PrivateClickMeasurement&&);
    void getSignedUnlinkableTokenForDestination(SourceSite&&, AttributionDestinationSite&&, AttributionTriggerData&&, const ApplicationBundleIdentifier&);
    void insertPrivateClickMeasurement(PrivateClickMeasurement&&, PrivateClickMeasurementAttributionType, CompletionHandler<void()>&&);
    void clearSentAttribution(PrivateClickMeasurement&&, WebCore::PCM::AttributionReportEndpoint);
    void attribute(SourceSite&&, AttributionDestinationSite&&, AttributionTriggerData&&, const ApplicationBundleIdentifier&);
    void fireConversionRequest(const PrivateClickMeasurement&, WebCore::PCM::AttributionReportEndpoint);
    void fireConversionRequestImpl(const PrivateClickMeasurement&, WebCore::PCM::AttributionReportEndpoint);
    void clearExpired();
    bool featureEnabled() const;
    bool debugModeEnabled() const;
    Seconds randomlyBetweenFifteenAndThirtyMinutes() const;

    RunLoop::Timer m_firePendingAttributionRequestsTimer;
    bool m_isRunningTest { false };
    std::optional<URL> m_tokenPublicKeyURLForTesting;
    std::optional<URL> m_tokenSignatureURLForTesting;
    std::optional<ApplicationBundleIdentifier> m_privateClickMeasurementAppBundleIDForTesting;
    mutable RefPtr<PCM::Store> m_store;
    String m_storageDirectory;
    UniqueRef<PCM::Client> m_client;

    struct AttributionReportTestConfig {
        URL attributionReportClickSourceURL;
        URL attributionReportClickDestinationURL;
    };

    std::optional<AttributionReportTestConfig> m_attributionReportTestConfig;

    struct TestingFraudPreventionValues {
        String unlinkableTokenForSource;
        String secretTokenForSource;
        String signatureForSource;
        String keyIDForSource;
        String unlinkableTokenForDestination;
        String secretTokenForDestination;
        String signatureForDestination;
        String keyIDForDestination;
    };

    std::optional<TestingFraudPreventionValues> m_fraudPreventionValuesForTesting;
};
    
} // namespace WebKit
