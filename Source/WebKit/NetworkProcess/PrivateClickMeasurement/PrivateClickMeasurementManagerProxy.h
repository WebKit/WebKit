/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "PrivateClickMeasurementConnection.h"
#include "PrivateClickMeasurementManagerInterface.h"
#include <wtf/FastMalloc.h>
#include <wtf/text/CString.h>

namespace WebKit {

class NetworkSession;

namespace PCM {

class ManagerProxy : public ManagerInterface {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ManagerProxy(const String& machServiceName, NetworkSession&);

    using ApplicationBundleIdentifier = String;

    void storeUnattributed(WebCore::PrivateClickMeasurement&&, CompletionHandler<void()>&&) final;
    void handleAttribution(WebCore::PCM::AttributionTriggerData&&, const URL& requestURL, WebCore::RegistrableDomain&& redirectDomain, const URL& firstPartyURL, const ApplicationBundleIdentifier&) final;
    void clear(CompletionHandler<void()>&&) final;
    void clearForRegistrableDomain(WebCore::RegistrableDomain&&, CompletionHandler<void()>&&) final;
    void migratePrivateClickMeasurementFromLegacyStorage(WebCore::PrivateClickMeasurement&&, PrivateClickMeasurementAttributionType) final;
    void setDebugModeIsEnabled(bool) final;

    void toStringForTesting(CompletionHandler<void(String)>&&) const final;
    void setOverrideTimerForTesting(bool) final;
    void setTokenPublicKeyURLForTesting(URL&&) final;
    void setTokenSignatureURLForTesting(URL&&) final;
    void setAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL) final;
    void markAllUnattributedAsExpiredForTesting() final;
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&) final;
    void setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID) final;
    void startTimerImmediatelyForTesting() final;
    void setPrivateClickMeasurementAppBundleIDForTesting(ApplicationBundleIdentifier&&) final;
    void destroyStoreForTesting(CompletionHandler<void()>&&) final;
    void allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo&) final;

private:
    template<MessageType messageType, typename... Args>
    void sendMessage(Args&&...) const;
    template<MessageType messageType, typename... Args, typename... ReplyArgs>
    void sendMessageWithReply(CompletionHandler<void(ReplyArgs...)>&&, Args&&...) const;

    Connection m_connection;
};

} // namespace PCM

} // namespace WebKit
