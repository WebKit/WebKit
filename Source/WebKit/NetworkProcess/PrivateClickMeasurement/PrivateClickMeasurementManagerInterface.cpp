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
#include "PrivateClickMeasurementManagerInterface.h"

#include "HandleMessage.h"
#include "PrivateClickMeasurementConnection.h"
#include "PrivateClickMeasurementDaemonClient.h"
#include "PrivateClickMeasurementDecoder.h"
#include "PrivateClickMeasurementEncoder.h"
#include "PrivateClickMeasurementManager.h"
#include "WebCoreArgumentCoders.h"

namespace WebKit {

namespace PCM {

namespace MessageInfo {

#define FUNCTION(mf) struct mf { static constexpr auto MemberFunction = &PrivateClickMeasurementManager::mf;
#define ARGUMENTS(...) using ArgsTuple = std::tuple<__VA_ARGS__>;
#define REPLY(...) using Reply = CompletionHandler<void(__VA_ARGS__)>; \
    static PCM::EncodedMessage encodeReply(__VA_ARGS__);
#define END };

FUNCTION(storeUnattributed)
ARGUMENTS(WebCore::PrivateClickMeasurement)
END

FUNCTION(handleAttribution)
ARGUMENTS(PrivateClickMeasurementManager::AttributionTriggerData, URL, WebCore::RegistrableDomain, URL)
END

FUNCTION(clear)
ARGUMENTS()
REPLY()
END

FUNCTION(clearForRegistrableDomain)
ARGUMENTS(WebCore::RegistrableDomain)
REPLY()
END

FUNCTION(migratePrivateClickMeasurementFromLegacyStorage)
ARGUMENTS(WebCore::PrivateClickMeasurement, PrivateClickMeasurementAttributionType)
END

FUNCTION(toStringForTesting)
ARGUMENTS()
REPLY(String)
END

FUNCTION(setOverrideTimerForTesting)
ARGUMENTS(bool)
END

FUNCTION(setTokenPublicKeyURLForTesting)
ARGUMENTS(URL)
END

FUNCTION(setTokenSignatureURLForTesting)
ARGUMENTS(URL)
END

FUNCTION(setAttributionReportURLsForTesting)
ARGUMENTS(URL, URL)
END

FUNCTION(markAllUnattributedAsExpiredForTesting)
ARGUMENTS()
END

FUNCTION(markAttributedPrivateClickMeasurementsAsExpiredForTesting)
ARGUMENTS()
REPLY()
END

FUNCTION(setEphemeralMeasurementForTesting)
ARGUMENTS(bool)
END

FUNCTION(setPCMFraudPreventionValuesForTesting)
ARGUMENTS(String, String, String, String)
END

FUNCTION(startTimerImmediatelyForTesting)
ARGUMENTS()
END

FUNCTION(destroyStoreForTesting)
ARGUMENTS()
REPLY()
END

FUNCTION(allowTLSCertificateChainForLocalPCMTesting)
ARGUMENTS(WebCore::CertificateInfo)
END

#undef FUNCTION
#undef ARGUMENTS
#undef REPLY
#undef END

#define EMPTY_REPLY(mf) PCM::EncodedMessage mf::encodeReply() { return { }; }
EMPTY_REPLY(clear);
EMPTY_REPLY(clearForRegistrableDomain);
EMPTY_REPLY(markAttributedPrivateClickMeasurementsAsExpiredForTesting);
EMPTY_REPLY(destroyStoreForTesting);
#undef EMPTY_REPLY

PCM::EncodedMessage toStringForTesting::encodeReply(String reply)
{
    PCM::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

} // namespace MessageInfo

bool messageTypeSendsReply(MessageType messageType)
{
    switch (messageType) {
    case MessageType::StoreUnattributed:
    case MessageType::HandleAttribution:
    case MessageType::MigratePrivateClickMeasurementFromLegacyStorage:
    case MessageType::SetOverrideTimerForTesting:
    case MessageType::SetTokenPublicKeyURLForTesting:
    case MessageType::SetTokenSignatureURLForTesting:
    case MessageType::SetAttributionReportURLsForTesting:
    case MessageType::MarkAllUnattributedAsExpiredForTesting:
    case MessageType::SetEphemeralMeasurementForTesting:
    case MessageType::SetPCMFraudPreventionValuesForTesting:
    case MessageType::StartTimerImmediatelyForTesting:
    case MessageType::AllowTLSCertificateChainForLocalPCMTesting:
        return false;
    case MessageType::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting:
    case MessageType::DestroyStoreForTesting:
    case MessageType::ToStringForTesting:
    case MessageType::Clear:
    case MessageType::ClearForRegistrableDomain:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static std::unique_ptr<PrivateClickMeasurementManager>& managerPointer()
{
    static NeverDestroyed<std::unique_ptr<PrivateClickMeasurementManager>> manager;
    return manager.get();
}

void initializePCMStorageInDirectory(const String& storageDirectory)
{
    ASSERT(!managerPointer());
    managerPointer() = makeUnique<PrivateClickMeasurementManager>(makeUniqueRef<PCM::DaemonClient>(), storageDirectory);
}

static PrivateClickMeasurementManager& daemonManager()
{
    ASSERT(managerPointer());
    return *managerPointer();
}

template<typename Info>
void handlePCMMessage(PCM::EncodedMessage&& encodedMessage)
{
    PCM::Decoder decoder(WTFMove(encodedMessage));

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    IPC::callMemberFunction(WTFMove(*arguments), &daemonManager(), Info::MemberFunction);
}

template<typename Info>
void handlePCMMessageWithReply(PCM::EncodedMessage&& encodedMessage, CompletionHandler<void(PCM::EncodedMessage&&)>&& replySender)
{
    PCM::Decoder decoder(WTFMove(encodedMessage));

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    typename Info::Reply completionHandler { [replySender = WTFMove(replySender)] (auto&&... args) mutable {
        replySender(Info::encodeReply(args...));
    }};

    IPC::callMemberFunction(WTFMove(*arguments), WTFMove(completionHandler), &daemonManager(), Info::MemberFunction);
}

void decodeMessageAndSendToManager(MessageType messageType, Vector<uint8_t>&& encodedMessage, CompletionHandler<void(Vector<uint8_t>&&)>&& replySender)
{
    ASSERT(messageTypeSendsReply(messageType) == !!replySender);
    switch (messageType) {
    case PCM::MessageType::StoreUnattributed:
        handlePCMMessage<MessageInfo::storeUnattributed>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::HandleAttribution:
        handlePCMMessage<MessageInfo::handleAttribution>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::Clear:
        handlePCMMessageWithReply<MessageInfo::clear>(WTFMove(encodedMessage), WTFMove(replySender));
        break;
    case PCM::MessageType::ClearForRegistrableDomain:
        handlePCMMessageWithReply<MessageInfo::clearForRegistrableDomain>(WTFMove(encodedMessage), WTFMove(replySender));
        break;
    case PCM::MessageType::MigratePrivateClickMeasurementFromLegacyStorage:
        handlePCMMessage<MessageInfo::migratePrivateClickMeasurementFromLegacyStorage>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::ToStringForTesting:
        handlePCMMessageWithReply<MessageInfo::toStringForTesting>(WTFMove(encodedMessage), WTFMove(replySender));
        break;
    case PCM::MessageType::SetOverrideTimerForTesting:
        handlePCMMessage<MessageInfo::setOverrideTimerForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::SetTokenPublicKeyURLForTesting:
        handlePCMMessage<MessageInfo::setTokenPublicKeyURLForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::SetTokenSignatureURLForTesting:
        handlePCMMessage<MessageInfo::setTokenSignatureURLForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::SetAttributionReportURLsForTesting:
        handlePCMMessage<MessageInfo::setAttributionReportURLsForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::MarkAllUnattributedAsExpiredForTesting:
        handlePCMMessage<MessageInfo::markAllUnattributedAsExpiredForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting:
        handlePCMMessageWithReply<MessageInfo::markAttributedPrivateClickMeasurementsAsExpiredForTesting>(WTFMove(encodedMessage), WTFMove(replySender));
        break;
    case PCM::MessageType::SetEphemeralMeasurementForTesting:
        handlePCMMessage<MessageInfo::setEphemeralMeasurementForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::SetPCMFraudPreventionValuesForTesting:
        handlePCMMessage<MessageInfo::setPCMFraudPreventionValuesForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::StartTimerImmediatelyForTesting:
        handlePCMMessage<MessageInfo::startTimerImmediatelyForTesting>(WTFMove(encodedMessage));
        break;
    case PCM::MessageType::DestroyStoreForTesting:
        handlePCMMessageWithReply<MessageInfo::destroyStoreForTesting>(WTFMove(encodedMessage), WTFMove(replySender));
        break;
    case PCM::MessageType::AllowTLSCertificateChainForLocalPCMTesting:
        handlePCMMessage<MessageInfo::allowTLSCertificateChainForLocalPCMTesting>(WTFMove(encodedMessage));
        break;
    }
}

} // namespace PCM

} // namespace WebKit
