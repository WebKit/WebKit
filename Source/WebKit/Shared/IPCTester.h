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


#if ENABLE(IPC_TESTING_API)

#include "IPCConnectionTesterIdentifier.h"
#include "IPCStreamTesterIdentifier.h"
#include "MessageReceiver.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedMemory.h"
#include "StreamConnectionBuffer.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include <atomic>
#include <wtf/HashMap.h>
#include <wtf/WorkQueue.h>

#endif

namespace WebKit {

#if ENABLE(IPC_TESTING_API)

class IPCConnectionTester;
class IPCStreamTester;

// Main test interface for initiating various IPC test activities.
class IPCTester final : public IPC::MessageReceiver {
public:
    IPCTester();
    ~IPCTester();

    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;
private:
    // Messages.
    void startMessageTesting(IPC::Connection&, String&& driverName);
    void stopMessageTesting(CompletionHandler<void()>&&);
    void createStreamTester(IPCStreamTesterIdentifier, IPC::StreamServerConnection::Handle&&);
    void releaseStreamTester(IPCStreamTesterIdentifier, CompletionHandler<void()>&&);
    void createConnectionTester(IPC::Connection&, IPCConnectionTesterIdentifier, IPC::Connection::Handle&&);
    void createConnectionTesterAndSendAsyncMessages(IPC::Connection&, IPCConnectionTesterIdentifier, IPC::Connection::Handle&&, uint32_t messageCount);
    void releaseConnectionTester(IPCConnectionTesterIdentifier, CompletionHandler<void()>&&);
    void sendSameSemaphoreBack(IPC::Connection&, IPC::Semaphore&&);
    void sendSemaphoreBackAndSignalProtocol(IPC::Connection&, IPC::Semaphore&&);
    void sendAsyncMessageToReceiver(IPC::Connection&, uint32_t);

    void stopIfNeeded();

    RefPtr<WorkQueue> m_testQueue;
    std::atomic<bool> m_shouldStop { false };

    using StreamTesterMap = HashMap<IPCStreamTesterIdentifier, IPC::ScopedActiveMessageReceiveQueue<IPCStreamTester>>;
    StreamTesterMap m_streamTesters;

    using ConnectionTesterMap = HashMap<IPCConnectionTesterIdentifier, IPC::ScopedActiveMessageReceiveQueue<IPCConnectionTester>>;
    ConnectionTesterMap m_connectionTesters;
};

#endif

}
