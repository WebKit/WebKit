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

#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include <WebCore/PrivateClickMeasurement.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceResponse.h>
#include <WebCore/Timer.h>
#include <pal/SessionID.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

enum class PrivateClickMeasurementAttributionType : bool { Unattributed, Attributed };

class PrivateClickMeasurementManager : public CanMakeWeakPtr<PrivateClickMeasurementManager> {
    WTF_MAKE_FAST_ALLOCATED;
public:

    using RegistrableDomain = WebCore::RegistrableDomain;
    using PrivateClickMeasurement = WebCore::PrivateClickMeasurement;
    using SourceSite = WebCore::PrivateClickMeasurement::SourceSite;
    using AttributeOnSite = WebCore::PrivateClickMeasurement::AttributeOnSite;
    using AttributionTriggerData = WebCore::PrivateClickMeasurement::AttributionTriggerData;

    explicit PrivateClickMeasurementManager(NetworkSession&, NetworkProcess&, PAL::SessionID);

    void storeUnattributed(PrivateClickMeasurement&&);
    void handleAttribution(AttributionTriggerData&&, const URL& requestURL, const WebCore::ResourceRequest& redirectRequest);
    void clear();
    void clearForRegistrableDomain(const RegistrableDomain&);
    void toString(CompletionHandler<void(String)>&&) const;
    void setPingLoadFunction(Function<void(NetworkResourceLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&&)>&& pingLoadFunction) { m_pingLoadFunction = WTFMove(pingLoadFunction); }
    void setOverrideTimerForTesting(bool value) { m_isRunningTest = value; }
    void setTokenPublicKeyURLForTesting(URL&&);
    void setTokenSignatureURLForTesting(URL&&);
    void setAttributionReportURLForTesting(URL&&);
    void markAllUnattributedAsExpiredForTesting();
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&);
    void setFraudPreventionValuesForTesting(String&& secretToken, String&& unlinkableToken, String&& signature, String&& keyID);
    void startTimer(Seconds);

private:
    void getTokenPublicKey(PrivateClickMeasurement&&, Function<void(PrivateClickMeasurement&& attribution, const String& publicKeyBase64URL)>&&);
    void getSignedSecretToken(PrivateClickMeasurement&&);
    void clearSentAttribution(PrivateClickMeasurement&&);
    void attribute(const SourceSite&, const AttributeOnSite&, AttributionTriggerData&&);
    void fireConversionRequest(const PrivateClickMeasurement&);
    void fireConversionRequestImpl(const PrivateClickMeasurement&);
    void firePendingAttributionRequests();
    void clearExpired();
    bool featureEnabled() const;
    bool debugModeEnabled() const;

    WebCore::Timer m_firePendingAttributionRequestsTimer;
    bool m_isRunningTest { false };
    Optional<URL> m_tokenPublicKeyURLForTesting;
    Optional<URL> m_tokenSignatureURLForTesting;
    Optional<URL> m_attributionReportBaseURLForTesting;
    WeakPtr<NetworkSession> m_networkSession;
    Ref<NetworkProcess> m_networkProcess;
    PAL::SessionID m_sessionID;
    Function<void(NetworkResourceLoadParameters&&, CompletionHandler<void(const WebCore::ResourceError&, const WebCore::ResourceResponse&)>&&)> m_pingLoadFunction;

    struct TestingFraudPreventionValues {
        String secretToken;
        String unlinkableToken;
        String signature;
        String keyID;
    };

    Optional<TestingFraudPreventionValues> m_fraudPreventionValuesForTesting;
};
    
} // namespace WebKit
