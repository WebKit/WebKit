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

#include "config.h"
#include "PrivateClickMeasurementManagerProxy.h"

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "PrivateClickMeasurementConnection.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit::PCM {

template<MessageType messageType, typename... Args>
void ManagerProxy::sendMessage(Args&&... args) const
{
    Daemon::Encoder encoder;
    encoder.encode(std::forward<Args>(args)...);
    m_connection.send(messageType, encoder.takeBuffer());
}

template<typename... Args> struct ReplyCaller;
template<> struct ReplyCaller<> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void()>&& completionHandler)
    {
        completionHandler();
    }
};
template<> struct ReplyCaller<String> {
    static void callReply(Daemon::Decoder&& decoder, CompletionHandler<void(String&&)>&& completionHandler)
    {
        std::optional<String> string;
        decoder >> string;
        if (!string)
            return completionHandler({ });
        completionHandler(WTFMove(*string));
    }
};

template<MessageType messageType, typename... Args, typename... ReplyArgs>
void ManagerProxy::sendMessageWithReply(CompletionHandler<void(ReplyArgs...)>&& completionHandler, Args&&... args) const
{
    Daemon::Encoder encoder;
    encoder.encode(std::forward<Args>(args)...);
    m_connection.sendWithReply(messageType, encoder.takeBuffer(), [completionHandler = WTFMove(completionHandler)] (auto replyBuffer) mutable {
        Daemon::Decoder decoder(WTFMove(replyBuffer));
        ReplyCaller<ReplyArgs...>::callReply(WTFMove(decoder), WTFMove(completionHandler));
    });
}

ManagerProxy::ManagerProxy(const String& machServiceName, NetworkSession& networkSession)
    : m_connection(machServiceName.utf8(), networkSession) { }

void ManagerProxy::storeUnattributed(WebCore::PrivateClickMeasurement&& pcm, CompletionHandler<void()>&& completionHandler)
{
    sendMessageWithReply<MessageType::StoreUnattributed>(WTFMove(completionHandler), pcm);
}

void ManagerProxy::handleAttribution(WebCore::PCM::AttributionTriggerData&& triggerData, const URL& requestURL, WebCore::RegistrableDomain&& redirectDomain, const URL& firstPartyURL, const ApplicationBundleIdentifier& applicationBundleIdentifier)
{
    sendMessage<MessageType::HandleAttribution>(triggerData, requestURL, redirectDomain, firstPartyURL, applicationBundleIdentifier);
}

void ManagerProxy::clear(CompletionHandler<void()>&& completionHandler)
{
    sendMessageWithReply<MessageType::Clear>(WTFMove(completionHandler));
}

void ManagerProxy::clearForRegistrableDomain(WebCore::RegistrableDomain&& domain, CompletionHandler<void()>&& completionHandler)
{
    sendMessageWithReply<MessageType::ClearForRegistrableDomain>(WTFMove(completionHandler), domain);
}

void ManagerProxy::setDebugModeIsEnabled(bool enabled)
{
    sendMessage<MessageType::SetDebugModeIsEnabled>(enabled);
}

void ManagerProxy::migratePrivateClickMeasurementFromLegacyStorage(WebCore::PrivateClickMeasurement&& pcm, PrivateClickMeasurementAttributionType type)
{
    sendMessage<MessageType::MigratePrivateClickMeasurementFromLegacyStorage>(pcm, type);
}

void ManagerProxy::toStringForTesting(CompletionHandler<void(String)>&& completionHandler) const
{
    sendMessageWithReply<MessageType::ToStringForTesting>(WTFMove(completionHandler));
}

void ManagerProxy::setOverrideTimerForTesting(bool value)
{
    sendMessage<MessageType::SetOverrideTimerForTesting>(value);
}

void ManagerProxy::setTokenPublicKeyURLForTesting(URL&& url)
{
    sendMessage<MessageType::SetTokenPublicKeyURLForTesting>(url);
}

void ManagerProxy::setTokenSignatureURLForTesting(URL&& url)
{
    sendMessage<MessageType::SetTokenSignatureURLForTesting>(url);
}

void ManagerProxy::setAttributionReportURLsForTesting(URL&& sourceURL, URL&& destinationURL)
{
    sendMessage<MessageType::SetAttributionReportURLsForTesting>(sourceURL, destinationURL);
}

void ManagerProxy::markAllUnattributedAsExpiredForTesting()
{
    sendMessage<MessageType::MarkAllUnattributedAsExpiredForTesting>();
}

void ManagerProxy::markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&& completionHandler)
{
    sendMessageWithReply<MessageType::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting>(WTFMove(completionHandler));
}

void ManagerProxy::setPCMFraudPreventionValuesForTesting(String&& unlinkableToken, String&& secretToken, String&& signature, String&& keyID)
{
    sendMessage<MessageType::SetPCMFraudPreventionValuesForTesting>(unlinkableToken, secretToken, signature, keyID);
}

void ManagerProxy::startTimerImmediatelyForTesting()
{
    sendMessage<MessageType::StartTimerImmediatelyForTesting>();
}

void ManagerProxy::setPrivateClickMeasurementAppBundleIDForTesting(ApplicationBundleIdentifier&& appBundleID)
{
    sendMessage<MessageType::SetPrivateClickMeasurementAppBundleIDForTesting>(appBundleID);
}

void ManagerProxy::destroyStoreForTesting(CompletionHandler<void()>&& completionHandler)
{
    sendMessageWithReply<MessageType::DestroyStoreForTesting>(WTFMove(completionHandler));
}

void ManagerProxy::allowTLSCertificateChainForLocalPCMTesting(const WebCore::CertificateInfo& certificateInfo)
{
    sendMessage<MessageType::AllowTLSCertificateChainForLocalPCMTesting>(certificateInfo);
}

} // namespace WebKit::PCM
