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

#ifndef MessageRecorder_h
#define MessageRecorder_h

#if HAVE(DTRACE)

#include "ProcessType.h"
#include <uuid/uuid.h>
#include <wtf/Forward.h>
#include <wtf/MallocPtr.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

struct WebKitMessageRecord {
    uint8_t sourceProcessType; // IPC::ProcessType
    pid_t sourceProcessID;

    uint8_t destinationProcessType; // IPC::ProcessType
    pid_t destinationProcessID;

    MallocPtr<char> messageReceiverName;
    MallocPtr<char> messageName;
    uint64_t destinationID;

    uuid_t UUID;

    double startTime;
    double endTime;

    bool isSyncMessage;
    bool shouldDispatchMessageWhenWaitingForSyncReply;
    bool isIncoming;
};

namespace IPC {

class Connection;
class MessageDecoder;
class MessageEncoder;

class MessageRecorder {
public:
    static bool isEnabled();

    class MessageProcessingToken {
        WTF_MAKE_NONCOPYABLE(MessageProcessingToken);
    public:
        explicit MessageProcessingToken(WebKitMessageRecord);
        ~MessageProcessingToken();

    private:
        WebKitMessageRecord m_record;
    };

    static std::unique_ptr<MessageRecorder::MessageProcessingToken> recordOutgoingMessage(IPC::Connection&, IPC::MessageEncoder&);
    static void recordIncomingMessage(IPC::Connection&, IPC::MessageDecoder&);

private:
    explicit MessageRecorder() { }
};

};

#endif // HAVE(DTRACE)

#endif // MessageRecorder_h
