/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef MessageDecoder_h
#define MessageDecoder_h

#include "ArgumentDecoder.h"
#include "MessageRecorder.h"
#include "StringReference.h"

#if HAVE(DTRACE)
#include <uuid/uuid.h>
#endif

#if HAVE(QOS_CLASSES)
#include <pthread/qos.h>
#endif

namespace IPC {

class DataReference;
class ImportanceAssertion;

class MessageDecoder : public ArgumentDecoder {
public:
    MessageDecoder(const DataReference& buffer, Vector<Attachment>);
    virtual ~MessageDecoder();

    StringReference messageReceiverName() const { return m_messageReceiverName; }
    StringReference messageName() const { return m_messageName; }
    uint64_t destinationID() const { return m_destinationID; }

    bool isSyncMessage() const;
    bool shouldDispatchMessageWhenWaitingForSyncReply() const;
    bool shouldUseFullySynchronousModeForTesting() const;

#if PLATFORM(MAC)
    void setImportanceAssertion(std::unique_ptr<ImportanceAssertion>);
#endif

#if HAVE(QOS_CLASSES)
    void setQOSClassOverride(pthread_override_t override) { m_qosClassOverride = override; }
#endif

#if HAVE(DTRACE)
    void setMessageProcessingToken(std::unique_ptr<MessageRecorder::MessageProcessingToken> token) { m_processingToken = WTFMove(token); }

    const uuid_t& UUID() const { return m_UUID; }
#endif

    static std::unique_ptr<MessageDecoder> unwrapForTesting(MessageDecoder&);

private:
    uint8_t m_messageFlags;
    StringReference m_messageReceiverName;
    StringReference m_messageName;

    uint64_t m_destinationID;

#if HAVE(DTRACE)
    uuid_t m_UUID;
    std::unique_ptr<MessageRecorder::MessageProcessingToken> m_processingToken;
#endif

#if PLATFORM(MAC)
    std::unique_ptr<ImportanceAssertion> m_importanceAssertion;
#endif

#if HAVE(QOS_CLASSES)
    pthread_override_t m_qosClassOverride { nullptr };
#endif
};

} // namespace IPC

#endif // MessageDecoder_h
