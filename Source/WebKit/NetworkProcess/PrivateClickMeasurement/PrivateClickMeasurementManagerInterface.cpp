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

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "HandleMessage.h"
#include "PrivateClickMeasurementConnection.h"
#include "PrivateClickMeasurementDaemonClient.h"
#include "PrivateClickMeasurementManager.h"
#include "WebCoreArgumentCoders.h"

#if PLATFORM(COCOA)
#include "PCMDaemonConnectionSet.h"
#endif

namespace WebKit::PCM {

namespace MessageInfo {

#define FUNCTION(mf) struct mf { static constexpr auto MemberFunction = &PrivateClickMeasurementManager::mf;
#define ARGUMENTS(...) using ArgsTuple = std::tuple<__VA_ARGS__>;
#define REPLY(...) using Reply = CompletionHandler<void(__VA_ARGS__)>; \
    static PCM::EncodedMessage encodeReply(__VA_ARGS__);
#define END };

FUNCTION(storeUnattributed)
ARGUMENTS(WebCore::PrivateClickMeasurement)
REPLY()
END

FUNCTION(handleAttribution)
ARGUMENTS(PrivateClickMeasurementManager::AttributionTriggerData, URL, WebCore::RegistrableDomain, URL, String)
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

FUNCTION(setDebugModeIsEnabled)
ARGUMENTS(bool)
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

FUNCTION(setPCMFraudPreventionValuesForTesting)
ARGUMENTS(String, String, String, String)
END

FUNCTION(startTimerImmediatelyForTesting)
ARGUMENTS()
END

FUNCTION(setPrivateClickMeasurementAppBundleIDForTesting)
ARGUMENTS(String)
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
EMPTY_REPLY(storeUnattributed);
#undef EMPTY_REPLY

PCM::EncodedMessage toStringForTesting::encodeReply(String reply)
{
    Daemon::Encoder encoder;
    encoder << reply;
    return encoder.takeBuffer();
}

} // namespace MessageInfo

bool messageTypeSendsReply(MessageType messageType)
{
    switch (messageType) {
    case MessageType::HandleAttribution:
    case MessageType::MigratePrivateClickMeasurementFromLegacyStorage:
    case MessageType::SetDebugModeIsEnabled:
    case MessageType::SetOverrideTimerForTesting:
    case MessageType::SetTokenPublicKeyURLForTesting:
    case MessageType::SetTokenSignatureURLForTesting:
    case MessageType::SetAttributionReportURLsForTesting:
    case MessageType::MarkAllUnattributedAsExpiredForTesting:
    case MessageType::SetPCMFraudPreventionValuesForTesting:
    case MessageType::StartTimerImmediatelyForTesting:
    case MessageType::SetPrivateClickMeasurementAppBundleIDForTesting:
    case MessageType::AllowTLSCertificateChainForLocalPCMTesting:
        return false;
    case MessageType::StoreUnattributed:
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
void handlePCMMessage(Span<const uint8_t> encodedMessage)
{
    Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    IPC::callMemberFunction(&daemonManager(), Info::MemberFunction, WTFMove(*arguments));
}

static void handlePCMMessageSetDebugModeIsEnabled(const Daemon::Connection& connection, Span<const uint8_t> encodedMessage)
{
#if PLATFORM(COCOA)
    Daemon::Decoder decoder(encodedMessage);
    std::optional<bool> enabled;
    decoder >> enabled;
    if (UNLIKELY(!enabled))
        return;

    auto& connectionSet = DaemonConnectionSet::singleton();
    bool debugModeWasEnabled = connectionSet.debugModeEnabled();
    connectionSet.setConnectedNetworkProcessHasDebugModeEnabled(connection, *enabled);
    if (debugModeWasEnabled != connectionSet.debugModeEnabled())
        daemonManager().setDebugModeIsEnabled(*enabled);
#else
    UNUSED_PARAM(connection);
    UNUSED_PARAM(encodedMessage);
#endif
}

template<typename Info>
void handlePCMMessageWithReply(Span<const uint8_t> encodedMessage, CompletionHandler<void(PCM::EncodedMessage&&)>&& replySender)
{
    Daemon::Decoder decoder(encodedMessage);

    std::optional<typename Info::ArgsTuple> arguments;
    decoder >> arguments;
    if (UNLIKELY(!arguments))
        return;

    typename Info::Reply completionHandler { [replySender = WTFMove(replySender)] (auto&&... args) mutable {
        replySender(Info::encodeReply(args...));
    }};

    IPC::callMemberFunction(&daemonManager(), Info::MemberFunction, WTFMove(*arguments), WTFMove(completionHandler));
}

void doDailyActivityInManager()
{
    daemonManager().firePendingAttributionRequests();
}

void decodeMessageAndSendToManager(const Daemon::Connection& connection, MessageType messageType, Span<const uint8_t> encodedMessage, CompletionHandler<void(Vector<uint8_t>&&)>&& replySender)
{
    ASSERT(messageTypeSendsReply(messageType) == !!replySender);
    switch (messageType) {
    case PCM::MessageType::StoreUnattributed:
        handlePCMMessageWithReply<MessageInfo::storeUnattributed>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::HandleAttribution:
        handlePCMMessage<MessageInfo::handleAttribution>(encodedMessage);
        break;
    case PCM::MessageType::Clear:
        handlePCMMessageWithReply<MessageInfo::clear>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::ClearForRegistrableDomain:
        handlePCMMessageWithReply<MessageInfo::clearForRegistrableDomain>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::SetDebugModeIsEnabled:
        handlePCMMessageSetDebugModeIsEnabled(connection, encodedMessage);
        break;
    case PCM::MessageType::MigratePrivateClickMeasurementFromLegacyStorage:
        handlePCMMessage<MessageInfo::migratePrivateClickMeasurementFromLegacyStorage>(encodedMessage);
        break;
    case PCM::MessageType::ToStringForTesting:
        handlePCMMessageWithReply<MessageInfo::toStringForTesting>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::SetOverrideTimerForTesting:
        handlePCMMessage<MessageInfo::setOverrideTimerForTesting>(encodedMessage);
        break;
    case PCM::MessageType::SetTokenPublicKeyURLForTesting:
        handlePCMMessage<MessageInfo::setTokenPublicKeyURLForTesting>(encodedMessage);
        break;
    case PCM::MessageType::SetTokenSignatureURLForTesting:
        handlePCMMessage<MessageInfo::setTokenSignatureURLForTesting>(encodedMessage);
        break;
    case PCM::MessageType::SetAttributionReportURLsForTesting:
        handlePCMMessage<MessageInfo::setAttributionReportURLsForTesting>(encodedMessage);
        break;
    case PCM::MessageType::MarkAllUnattributedAsExpiredForTesting:
        handlePCMMessage<MessageInfo::markAllUnattributedAsExpiredForTesting>(encodedMessage);
        break;
    case PCM::MessageType::MarkAttributedPrivateClickMeasurementsAsExpiredForTesting:
        handlePCMMessageWithReply<MessageInfo::markAttributedPrivateClickMeasurementsAsExpiredForTesting>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::SetPCMFraudPreventionValuesForTesting:
        handlePCMMessage<MessageInfo::setPCMFraudPreventionValuesForTesting>(encodedMessage);
        break;
    case PCM::MessageType::StartTimerImmediatelyForTesting:
        handlePCMMessage<MessageInfo::startTimerImmediatelyForTesting>(encodedMessage);
        break;
    case PCM::MessageType::SetPrivateClickMeasurementAppBundleIDForTesting:
        handlePCMMessage<MessageInfo::setPrivateClickMeasurementAppBundleIDForTesting>(encodedMessage);
        break;
    case PCM::MessageType::DestroyStoreForTesting:
        handlePCMMessageWithReply<MessageInfo::destroyStoreForTesting>(encodedMessage, WTFMove(replySender));
        break;
    case PCM::MessageType::AllowTLSCertificateChainForLocalPCMTesting:
        handlePCMMessage<MessageInfo::allowTLSCertificateChainForLocalPCMTesting>(encodedMessage);
        break;
    }
}

} // namespace WebKit::PCM
