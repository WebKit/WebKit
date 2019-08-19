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
#include "RemoteObjectInvocation.h"

#include "ArgumentCoders.h"
#include "UserData.h"

namespace WebKit {

RemoteObjectInvocation::RemoteObjectInvocation()
{
}

RemoteObjectInvocation::RemoteObjectInvocation(const String& interfaceIdentifier, RefPtr<API::Dictionary>&& encodedInvocation, std::unique_ptr<ReplyInfo>&& replyInfo)
    : m_interfaceIdentifier(interfaceIdentifier)
    , m_encodedInvocation(WTFMove(encodedInvocation))
    , m_replyInfo(WTFMove(replyInfo))
{
}

void RemoteObjectInvocation::encode(IPC::Encoder& encoder) const
{
    encoder << m_interfaceIdentifier;
    UserData::encode(encoder, m_encodedInvocation.get());
    if (!m_replyInfo) {
        encoder << false;
        return;
    }

    encoder << true;
    encoder << m_replyInfo->replyID;
    encoder << m_replyInfo->blockSignature;
}

bool RemoteObjectInvocation::decode(IPC::Decoder& decoder, RemoteObjectInvocation& result)
{
    if (!decoder.decode(result.m_interfaceIdentifier))
        return false;

    RefPtr<API::Object> encodedInvocation;
    if (!UserData::decode(decoder, encodedInvocation))
        return false;

    if (!encodedInvocation || encodedInvocation->type() != API::Object::Type::Dictionary)
        return false;

    result.m_encodedInvocation = static_cast<API::Dictionary*>(encodedInvocation.get());

    bool hasReplyInfo;
    if (!decoder.decode(hasReplyInfo))
        return false;

    if (hasReplyInfo) {
        uint64_t replyID;
        if (!decoder.decode(replyID))
            return false;

        String blockSignature;
        if (!decoder.decode(blockSignature))
            return false;

        result.m_replyInfo = makeUnique<ReplyInfo>(replyID, WTFMove(blockSignature));
    }

    return true;
}

}
