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

namespace WebKit::WebPushD {

// If an origin processes more than this many silent pushes, then it will be unsubscribed from push.
constexpr unsigned maxSilentPushCount = 3;

constexpr const char* protocolVersionKey = "protocol version";
constexpr uint64_t protocolVersionValue = 2;
constexpr const char* protocolEncodedMessageKey = "encoded message";

constexpr const char* protocolDebugMessageKey { "debug message" };
constexpr const char* protocolDebugMessageLevelKey { "debug message level" };

constexpr const char* protocolMessageTypeKey { "message type" };

enum class MessageType : uint8_t {
    EchoTwice = 1,
    RequestSystemNotificationPermission,
    DeletePushAndNotificationRegistration,
    GetOriginsWithPushAndNotificationPermissions,
    SetDebugModeIsEnabled,
    UpdateConnectionConfiguration,
    InjectPushMessageForTesting,
    InjectEncryptedPushMessageForTesting,
    GetPendingPushMessages,
    SubscribeToPushService,
    UnsubscribeFromPushService,
    GetPushSubscription,
    GetPushPermissionState,
    IncrementSilentPushCount,
    RemoveAllPushSubscriptions,
    RemovePushSubscriptionsForOrigin,
    SetPublicTokenForTesting,
    SetPushAndNotificationsEnabledForOrigin,
};

enum class RawXPCMessageType : uint8_t {
    GetPushTopicsForTesting = 192,
};

inline bool messageTypeSendsReply(MessageType messageType)
{
    switch (messageType) {
    case MessageType::EchoTwice:
    case MessageType::GetOriginsWithPushAndNotificationPermissions:
    case MessageType::DeletePushAndNotificationRegistration:
    case MessageType::RequestSystemNotificationPermission:
    case MessageType::GetPendingPushMessages:
    case MessageType::InjectPushMessageForTesting:
    case MessageType::InjectEncryptedPushMessageForTesting:
    case MessageType::SubscribeToPushService:
    case MessageType::UnsubscribeFromPushService:
    case MessageType::GetPushSubscription:
    case MessageType::GetPushPermissionState:
    case MessageType::IncrementSilentPushCount:
    case MessageType::RemoveAllPushSubscriptions:
    case MessageType::RemovePushSubscriptionsForOrigin:
    case MessageType::SetPublicTokenForTesting:
    case MessageType::SetPushAndNotificationsEnabledForOrigin:
        return true;
    case MessageType::SetDebugModeIsEnabled:
    case MessageType::UpdateConnectionConfiguration:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

enum class DaemonMessageType : uint8_t {
    DebugMessage = 1,
};

inline bool daemonMessageTypeSendsReply(DaemonMessageType messageType)
{
    switch (messageType) {
    case DaemonMessageType::DebugMessage:
        return false;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebKit::WebPushD
