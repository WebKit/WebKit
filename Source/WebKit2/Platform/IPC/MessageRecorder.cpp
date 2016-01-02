/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "MessageRecorder.h"

#include "Connection.h"
#include "MessageDecoder.h"
#include "MessageEncoder.h"
#include "MessageRecorderProbes.h"
#include <wtf/CurrentTime.h>

namespace IPC {

bool MessageRecorder::isEnabled()
{
    return WEBKITMESSAGERECORDER_MESSAGE_RECEIVED_ENABLED() || WEBKITMESSAGERECORDER_MESSAGE_SENT_ENABLED();
}

std::unique_ptr<MessageRecorder::MessageProcessingToken> MessageRecorder::recordOutgoingMessage(Connection& connection, MessageEncoder& encoder)
{
    if (!isEnabled() || !connection.isValid())
        return nullptr;

    WebKitMessageRecord record;
    record.sourceProcessType = static_cast<uint64_t>(connection.client()->localProcessType());
    record.destinationProcessType = static_cast<uint64_t>(connection.client()->remoteProcessType());
    record.destinationID = encoder.destinationID();
    record.isSyncMessage = encoder.isSyncMessage();
    record.shouldDispatchMessageWhenWaitingForSyncReply = encoder.shouldDispatchMessageWhenWaitingForSyncReply();
    record.sourceProcessID = getpid();
    record.destinationProcessID = connection.remoteProcessID();
    record.isIncoming = false;

    record.messageReceiverName = MallocPtr<char>::malloc(sizeof(char) * (encoder.messageReceiverName().size() + 1));
    strncpy(record.messageReceiverName.get(), encoder.messageReceiverName().data(), encoder.messageReceiverName().size());
    record.messageReceiverName.get()[encoder.messageReceiverName().size()] = 0;

    record.messageName = MallocPtr<char>::malloc(sizeof(char) * (encoder.messageName().size() + 1));
    strncpy(record.messageName.get(), encoder.messageName().data(), encoder.messageName().size());
    record.messageName.get()[encoder.messageName().size()] = 0;

    uuid_copy(record.UUID, encoder.UUID());

    return std::make_unique<MessageProcessingToken>(WTFMove(record));
}

void MessageRecorder::recordIncomingMessage(Connection& connection, MessageDecoder& decoder)
{
    if (!isEnabled() || !connection.isValid())
        return;

    WebKitMessageRecord record;
    record.sourceProcessType = static_cast<uint64_t>(connection.client()->remoteProcessType());
    record.destinationProcessType = static_cast<uint64_t>(connection.client()->localProcessType());
    record.destinationID = decoder.destinationID();
    record.isSyncMessage = decoder.isSyncMessage();
    record.shouldDispatchMessageWhenWaitingForSyncReply = decoder.shouldDispatchMessageWhenWaitingForSyncReply();
    record.sourceProcessID = connection.remoteProcessID();
    record.destinationProcessID = getpid();
    record.isIncoming = true;

    record.messageReceiverName = MallocPtr<char>::malloc(sizeof(char) * (decoder.messageReceiverName().size() + 1));
    strncpy(record.messageReceiverName.get(), decoder.messageReceiverName().data(), decoder.messageReceiverName().size());
    record.messageReceiverName.get()[decoder.messageReceiverName().size()] = 0;

    record.messageName = MallocPtr<char>::malloc(sizeof(char) * (decoder.messageName().size() + 1));
    strncpy(record.messageName.get(), decoder.messageName().data(), decoder.messageName().size());
    record.messageName.get()[decoder.messageName().size()] = 0;

    uuid_copy(record.UUID, decoder.UUID());

    decoder.setMessageProcessingToken(std::make_unique<MessageProcessingToken>(WTFMove(record)));
}

#pragma mark MessageProcessingToken

MessageRecorder::MessageProcessingToken::MessageProcessingToken(WebKitMessageRecord record)
    : m_record(WTFMove(record))
{
    m_record.startTime = monotonicallyIncreasingTime();
}

MessageRecorder::MessageProcessingToken::~MessageProcessingToken()
{
    m_record.endTime = monotonicallyIncreasingTime();

    if (m_record.isIncoming)
        WEBKITMESSAGERECORDER_MESSAGE_RECEIVED(&m_record);
    else
        WEBKITMESSAGERECORDER_MESSAGE_SENT(&m_record);
}

}
