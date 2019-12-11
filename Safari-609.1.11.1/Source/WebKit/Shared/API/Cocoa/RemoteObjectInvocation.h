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

#ifndef RemoteObjectInvocation_h
#define RemoteObjectInvocation_h

#include "APIDictionary.h"
#include <wtf/text/WTFString.h>

namespace IPC {
class Encoder;
class Decoder;
}

namespace WebKit {

class RemoteObjectInvocation {
public:
    struct ReplyInfo {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        ReplyInfo(uint64_t replyID, String&& blockSignature)
            : replyID(replyID)
            , blockSignature(WTFMove(blockSignature))
        {
        }

        const uint64_t replyID;
        const String blockSignature;
    };
    RemoteObjectInvocation();
    RemoteObjectInvocation(const String& interfaceIdentifier, RefPtr<API::Dictionary>&& encodedInvocation, std::unique_ptr<ReplyInfo>&&);

    const String& interfaceIdentifier() const { return m_interfaceIdentifier; }
    const API::Dictionary* encodedInvocation() const { return m_encodedInvocation.get(); }
    const ReplyInfo* replyInfo() const { return m_replyInfo.get(); }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RemoteObjectInvocation&);

private:
    String m_interfaceIdentifier;
    RefPtr<API::Dictionary> m_encodedInvocation;
    std::unique_ptr<ReplyInfo> m_replyInfo;
};

}

#endif // RemoteObjectInvocation_h
