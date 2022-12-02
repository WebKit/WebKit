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

#include "config.h"
#include "IPCTester.h"

#if ENABLE(IPC_TESTING_API)
#include "Connection.h"
#include "Decoder.h"
#include "IPCConnectionTester.h"
#include "IPCStreamTester.h"
#include "IPCTesterMessages.h"
#include "IPCTesterReceiverMessages.h"
#include "IPCUtilities.h"

#include <atomic>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/threads/BinarySemaphore.h>

// The tester API.
extern "C" {
// Returns 0 if driver should continue.
typedef int (*WKMessageTestSendMessageFunc)(const uint8_t* data, size_t sz, void* context);
typedef void (*WKMessageTestDriverFunc)(WKMessageTestSendMessageFunc sendMessageFunc, void* context);
}

namespace {
struct SendMessageContext {
    IPC::Connection& connection;
    std::atomic<bool>& shouldStop;
};

}

extern "C" {

static void defaultTestDriver(WKMessageTestSendMessageFunc sendMessageFunc, void* context)
{
    Vector<uint8_t> data(1000);
    for (int i = 0; i < 1000; i++) {
        cryptographicallyRandomValues(data.data(), data.size());
        int ret = sendMessageFunc(data.data(), data.size(), context);
        if (ret)
            return;
    }
}

static int sendTestMessage(const uint8_t* data, size_t size, void* context)
{
    auto messageContext = reinterpret_cast<SendMessageContext*>(context);
    if (messageContext->shouldStop)
        return 1;
    auto& testedConnection = messageContext->connection;
    if (!testedConnection.isValid())
        return 1;
    BinarySemaphore semaphore;
    auto decoder = IPC::Decoder::create(data, size, [&semaphore] (const uint8_t*, size_t) { semaphore.signal(); }, { }); // NOLINT
    if (decoder) {
        testedConnection.dispatchIncomingMessageForTesting(WTFMove(decoder));
        semaphore.wait();
    }
    return 0;
}

}

namespace WebKit {

static WKMessageTestDriverFunc messageTestDriver(String&& driverName)
{
    if (driverName.isEmpty() || driverName == "default"_s)
        driverName = String::fromUTF8(getenv("WEBKIT_MESSAGE_TEST_DEFAULT_DRIVER"));
    if (driverName.isEmpty() || driverName == "default"_s)
        return defaultTestDriver;
    auto testDriver = reinterpret_cast<WKMessageTestDriverFunc>(dlsym(RTLD_DEFAULT, driverName.utf8().data()));
    RELEASE_ASSERT(testDriver);
    return testDriver;
}

static void runMessageTesting(IPC::Connection& connection, std::atomic<bool>& shouldStop, String&& driverName)
{
    connection.setIgnoreInvalidMessageForTesting();
    SendMessageContext context { connection, shouldStop };
    auto driver = messageTestDriver(WTFMove(driverName));
    driver(sendTestMessage, &context);
}

IPCTester::IPCTester() = default;

IPCTester::~IPCTester()
{
    stopIfNeeded();
}

void IPCTester::startMessageTesting(IPC::Connection& connection, String&& driverName)
{
    if (!m_testQueue)
        m_testQueue = WorkQueue::create("IPC testing work queue");
    m_testQueue->dispatch([connection = Ref { connection }, &shouldStop = m_shouldStop, driverName = WTFMove(driverName)]() mutable {
        IPC::startTestingIPC();
        runMessageTesting(connection, shouldStop, WTFMove(driverName));
        IPC::stopTestingIPC();
    });
}

void IPCTester::stopMessageTesting(CompletionHandler<void()> completionHandler)
{
    stopIfNeeded();
    completionHandler();
}

void IPCTester::createStreamTester(IPCStreamTesterIdentifier identifier, IPC::StreamServerConnection::Handle&& serverConnection)
{
    auto addResult = m_streamTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCStreamTester> { IPCStreamTester::create(identifier, WTFMove(serverConnection)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry || IPC::isTestingIPC());
}

void IPCTester::releaseStreamTester(IPCStreamTesterIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    m_streamTesters.remove(identifier);
    completionHandler();
}

void IPCTester::sendSameSemaphoreBack(IPC::Connection& connection, IPC::Semaphore&& semaphore)
{
    connection.send(Messages::IPCTester::SendSameSemaphoreBack(semaphore), 0);
}

void IPCTester::sendSemaphoreBackAndSignalProtocol(IPC::Connection& connection, IPC::Semaphore&& semaphore)
{
    IPC::Semaphore newSemaphore;
    connection.send(Messages::IPCTester::SendSemaphoreBackAndSignalProtocol(newSemaphore), 0);
    if (!semaphore.waitFor(10_s)) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    newSemaphore.signal();
    // Wait for protocol commit. Otherwise newSemaphore will be destroyed, and the waiter on the other side
    // will fail to wait.
    if (!semaphore.waitFor(10_s)) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
}

void IPCTester::sendAsyncMessageToReceiver(IPC::Connection& connection, uint32_t arg0)
{
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::AsyncMessage(arg0 + 1), [arg0](uint32_t newArg0) {
        ASSERT_UNUSED(arg0, newArg0 == arg0 + 2);
    }, 0);
}

void IPCTester::createConnectionTester(IPC::Connection& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& testedConnectionIdentifier)
{
    auto addResult = m_connectionTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCConnectionTester> { IPCConnectionTester::create(connection, identifier, WTFMove(testedConnectionIdentifier)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry || IPC::isTestingIPC());
}

void IPCTester::createConnectionTesterAndSendAsyncMessages(IPC::Connection& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& testedConnectionIdentifier, uint32_t messageCount)
{
    auto addResult = m_connectionTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCConnectionTester> { IPCConnectionTester::create(connection, identifier, WTFMove(testedConnectionIdentifier)) };
    });
    if (!addResult.isNewEntry) {
        ASSERT_IS_TESTING_IPC();
        return;
    }
    addResult.iterator->value->sendAsyncMessages(messageCount);
}

void IPCTester::releaseConnectionTester(IPCConnectionTesterIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    m_connectionTesters.remove(identifier);
    completionHandler();
}

void IPCTester::stopIfNeeded()
{
    if (m_testQueue) {
        m_shouldStop = true;
        m_testQueue->dispatchSync([] { });
        m_testQueue = nullptr;
    }
}

}

#endif
