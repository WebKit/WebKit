/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PrivateClickMeasurementConnection.h"

#import "DaemonEncoder.h"
#import "PrivateClickMeasurementXPCUtilities.h"
#import <wtf/NeverDestroyed.h>
#import <wtf/darwin/XPCExtras.h>

namespace WebKit::PCM {

void Connection::newConnectionWasInitialized() const
{
    ASSERT(m_connection);
    if (!m_networkSession
        || m_networkSession->sessionID().isEphemeral()
        || !m_networkSession->privateClickMeasurementDebugModeEnabled())
        return;

    Daemon::Encoder encoder;
    encoder.encode(true);
    send(MessageType::SetDebugModeIsEnabled, encoder.takeBuffer());
}

void Connection::connectionReceivedEvent(xpc_object_t request)
{
    if (xpc_get_type(request) != XPC_TYPE_DICTIONARY)
        return;
    String debugMessage = xpcDictionaryGetString(request, protocolDebugMessageKey);
    if (!debugMessage)
        return;
    auto messageLevel = static_cast<JSC::MessageLevel>(xpc_dictionary_get_uint64(request, protocolDebugMessageLevelKey));
    CheckedPtr networkSession = m_networkSession.get();
    if (!networkSession)
        return;
    m_networkSession->networkProcess().broadcastConsoleMessage(m_networkSession->sessionID(), MessageSource::PrivateClickMeasurement, messageLevel, debugMessage);
}

OSObjectPtr<xpc_object_t> Connection::dictionaryFromMessage(MessageType messageType, EncodedMessage&& message) const
{
    auto dictionary = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    addVersionAndEncodedMessageToDictionary(WTFMove(message), dictionary.get());
    xpc_dictionary_set_uint64(dictionary.get(), protocolMessageTypeKey, static_cast<uint64_t>(messageType));
    return dictionary;
}

} // namespace WebKit::PCM
