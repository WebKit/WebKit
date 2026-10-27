/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(IPC_TESTING_API) && !ENABLE(IPC_TESTING_SWIFT)

#include "IPCTesterPayload.h"
#include "MessageReceiver.h"
#include <wtf/CompletionHandler.h>

namespace IPC {
class Connection;
}

namespace WebKit {

// Proxy interface to test IPC activities related to receiving messages.
class IPCTesterReceiver final : public IPC::MessageReceiver, public RefCounted<IPCTesterReceiver> {
public:
    static Ref<IPCTesterReceiver> create();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    // IPC::MessageReceiver
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
private:
    IPCTesterReceiver() = default;

    // Messages
    void asyncMessage(uint32_t, CompletionHandler<void(uint32_t)>&&);
    void deferredReplyMessage(uint32_t, CompletionHandler<void(uint64_t)>&&);
    void completeDeferredReply(uint32_t);
    void transformInner(const IPCTesterInner&, CompletionHandler<void(IPCTesterInner&&)>&&);
    void transformInnerSwift(const IPCTesterInner&, CompletionHandler<void(IPCTesterInner&&)>&&);
    void transformPayloadSwift(const IPCTesterPayload&, CompletionHandler<void(IPCTesterPayload&&)>&&);
    void transformPayloadSynchronouslySwift(const IPCTesterPayload&, CompletionHandler<void(IPCTesterPayload&&)>&&);
    void transformNodeSwift(Ref<IPCTesterNode>&&, RefPtr<IPCTesterNode>&&, CompletionHandler<void(RefPtr<IPCTesterNode>&&, uint32_t)>&&);
    void describeSwift(IPCTesterColor, OptionSet<IPCTesterFlag>, IPCTesterChoice, IPCConnectionTesterIdentifier, const IPCTesterVariant&, bool, CompletionHandler<void(String&&)>&&);
    void halveEvenNumberSwift(IPC::Connection&, uint32_t, CompletionHandler<void(uint32_t)>&&);
    void recordSwift(const String&);
    void takeRecordedSwift(CompletionHandler<void(Vector<String>&&)>&&);
    void clearRecordedSwift(CompletionHandler<void()>&&);

    CompletionHandler<void(uint64_t)> m_deferredReply;
    uint32_t m_deferredReplyArgument { 0 };
    Vector<String> m_recordedTexts;
};

}

#endif
