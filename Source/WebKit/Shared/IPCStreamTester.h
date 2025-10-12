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

#if ENABLE(IPC_TESTING_API)

#include "IPCStreamTesterIdentifier.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <WebCore/SharedMemory.h>
#include <memory>
#include <wtf/HashMap.h>

namespace IPC {
class Connection;
class StreamConnectionBuffer;
class StreamConnectionWorkQueue;
}

namespace WebKit {

// Interface to test various IPC stream related activities.
class IPCStreamTester final : public IPC::StreamServerConnection::Client {
    WTF_MAKE_TZONE_ALLOCATED(IPCStreamTester);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(IPCStreamTester);
public:
    static RefPtr<IPCStreamTester> create(IPCStreamTesterIdentifier, IPC::StreamServerConnection::Handle&&, bool ignoreInvalidMessageForTesting);
    void stopListeningForIPC(Ref<IPCStreamTester>&& refFromConnection);

    // IPC::StreamMessageReceiver overrides.
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

private:
    IPCStreamTester(IPCStreamTesterIdentifier, IPC::StreamServerConnection::Handle&&, bool ignoreInvalidMessageForTesting);
    ~IPCStreamTester();
    void initialize();
    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }
    Ref<IPC::StreamConnectionWorkQueue> protectedWorkQueue() const { return m_workQueue; }

    // IPC::StreamServerConnection::Client overrides.
    void didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&) final;

    // Messages.
    void syncMessage(uint32_t value, CompletionHandler<void(uint32_t)>&&);
    void syncMessageNotStreamEncodableReply(uint32_t value, CompletionHandler<void(uint32_t)>&&);
    void syncMessageNotStreamEncodableBoth(uint32_t value, CompletionHandler<void(uint32_t)>&&);
    void syncMessageReturningSharedMemory1(uint32_t byteCount, CompletionHandler<void(std::optional<WebCore::SharedMemory::Handle>&&)>&&);
    void syncMessageEmptyReply(uint32_t, CompletionHandler<void()>&&);
    void syncCrashOnZero(int32_t, CompletionHandler<void(int32_t)>&&);
    void checkAutoreleasePool(CompletionHandler<void(int32_t)>&&);
    void asyncPing(uint32_t value, CompletionHandler<void(uint32_t)>&&);
    void emptyMessage();
    void checkInvalidMessages(CompletionHandler<void(uint32_t)>&&);

    const Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    const Ref<IPC::StreamServerConnection> m_streamConnection;
    const IPCStreamTesterIdentifier m_identifier;
    uint32_t m_invalidMessages { 0 };
    std::shared_ptr<bool> m_autoreleasePoolCheckValue;
};

}

#endif
