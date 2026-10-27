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
#include "IPCTesterPayload.h"
#include "IPCTesterReceiver.h"
#include "IPCTesterReceiverMessages.h"
#include "IPCUtilities.h"
#include "Logging.h"
#include "TestParameter.h"
#include "WebKit-Swift.h"
#include <WebCore/ExceptionData.h>
#include <atomic>
#include <dlfcn.h>
#include <stdio.h>
#include <unistd.h>
#include <wtf/Borrow.h>
#include <wtf/Box.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/threads/BinarySemaphore.h>

// The tester API.
extern "C" {
// Returns 0 if driver should continue.
typedef int (*WKMessageTestSendMessageFunc)(std::span<const uint8_t> buffer, void* context);
typedef void (*WKMessageTestDriverFunc)(WKMessageTestSendMessageFunc sendMessageFunc, void* context);
}

namespace {
struct SendMessageContext {
    Ref<IPC::Connection> connection;
    std::atomic<bool>& shouldStop;
};

}

extern "C" {

static void defaultTestDriver(WKMessageTestSendMessageFunc sendMessageFunc, void* context)
{
    Vector<uint8_t> data(1000);
    Borrow borrowedData = data;
    for (unsigned i = 0; i < 1000; ++i) {
        cryptographicallyRandomValues(borrowedData->mutableSpan());
        if (sendMessageFunc(borrowedData->span(), context))
            return;
    }
}

static int sendTestMessage(std::span<const uint8_t> buffer, void* context)
{
    auto messageContext = reinterpret_cast<SendMessageContext*>(context);
    if (messageContext->shouldStop)
        return 1;
    Ref testedConnection = messageContext->connection;
    if (!testedConnection->isValid())
        return 1;
    BinarySemaphore semaphore;
    auto decoder = IPC::Decoder::create(buffer, [&semaphore] (std::span<const uint8_t>) { semaphore.signal(); }, { }); // NOLINT
    if (decoder) {
        testedConnection->dispatchIncomingMessageForTesting(makeUniqueRefFromNonNullUniquePtr(WTF::move(decoder)));
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
    auto testDriver = reinterpret_cast<WKMessageTestDriverFunc>(dlsym(RTLD_DEFAULT, driverName.utf8().legacyCStringPointer()));
    RELEASE_ASSERT(testDriver);
    return testDriver;
}

static void runMessageTesting(IPC::Connection& connection, std::atomic<bool>& shouldStop, String&& driverName)
{
    connection.setIgnoreInvalidMessageForTesting();
    SendMessageContext context { connection, shouldStop };
    auto driver = messageTestDriver(WTF::move(driverName));
    driver(sendTestMessage, &context);
}

Ref<IPCTester> IPCTester::create()
{
    return adoptRef(*new IPCTester);
}

IPCTester::IPCTester() = default;

IPCTester::~IPCTester()
{
    stopIfNeeded();
}

void IPCTester::startMessageTesting(IPC::Connection& connection, String&& driverName)
{
    if (!m_testQueue)
        m_testQueue = WorkQueue::create("IPC testing work queue"_s);
    m_testQueue->dispatch([connection = Ref { connection }, &shouldStop = m_shouldStop, driverName = WTF::move(driverName)]() mutable {
        IPC::startTestingIPC();
        runMessageTesting(connection, shouldStop, WTF::move(driverName));
        IPC::stopTestingIPC();
    });
}

void IPCTester::stopMessageTesting(CompletionHandler<void()>&& completionHandler)
{
    stopIfNeeded();
    completionHandler();
}

void IPCTester::createStreamTester(IPC::Connection& connection, IPCStreamTesterIdentifier identifier, IPC::StreamServerConnection::Handle&& serverConnection)
{
    auto addResult = m_streamTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCStreamTester> { IPCStreamTester::create(identifier, WTF::move(serverConnection), connection.ignoreInvalidMessageForTesting()) };
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

void IPCTester::sendAsyncMessageToReceiverRequestingReply(IPC::Connection& connection, uint32_t arg0, CompletionHandler<void(uint32_t, bool)>&& completionHandler)
{
#if ENABLE(IPC_TESTING_SWIFT)
    constexpr bool usingSwift = true;
#else
    constexpr bool usingSwift = false;
#endif
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::AsyncMessage(arg0 + 1), [completionHandler = WTF::move(completionHandler)](uint32_t newArg0) mutable {
        completionHandler(newArg0, usingSwift);
    }, 0);
}

void IPCTester::sendDeferredReplyMessageToReceiver(IPC::Connection& connection, uint32_t arg0, CompletionHandler<void(uint64_t, bool)>&& completionHandler)
{
#if ENABLE(IPC_TESTING_SWIFT)
    constexpr bool usingSwift = true;
#else
    constexpr bool usingSwift = false;
#endif
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::DeferredReplyMessage(arg0 + 1), [completionHandler = WTF::move(completionHandler)](uint64_t newArg0) mutable {
        completionHandler(newArg0, usingSwift);
    }, 0);
}

void IPCTester::completeDeferredReplyOnReceiver(IPC::Connection& connection, uint32_t arg0)
{
    connection.send(Messages::IPCTesterReceiver::CompleteDeferredReply(arg0), 0);
}

void IPCTester::createConnectionTester(IPC::Connection& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& testedConnectionIdentifier)
{
    auto addResult = m_connectionTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCConnectionTester> { IPCConnectionTester::create(connection, identifier, WTF::move(testedConnectionIdentifier)) };
    });
    ASSERT_UNUSED(addResult, addResult.isNewEntry || IPC::isTestingIPC());
}

void IPCTester::createConnectionTesterAndSendAsyncMessages(IPC::Connection& connection, IPCConnectionTesterIdentifier identifier, IPC::Connection::Handle&& testedConnectionIdentifier, uint32_t messageCount)
{
    auto addResult = m_connectionTesters.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue<IPCConnectionTester> { IPCConnectionTester::create(connection, identifier, WTF::move(testedConnectionIdentifier)) };
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

void IPCTester::asyncPing(uint32_t value, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    completionHandler(value + 1);
}

void IPCTester::syncPing(IPC::Connection&, uint32_t value, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    completionHandler(value + 1);
}

void IPCTester::syncPingEmptyReply(IPC::Connection&, uint32_t value, CompletionHandler<void()>&& completionHandler)
{
    UNUSED_PARAM(value);
    completionHandler();
}

void IPCTester::asyncOptionalExceptionData(IPC::Connection&, bool sendEngaged, CompletionHandler<void(std::optional<WebCore::ExceptionData>, String)>&& completionHandler)
{
    if (sendEngaged) {
        completionHandler(WebCore::ExceptionData { WebCore::ExceptionCode::WrongDocumentError, "m"_s }, "a"_s);
        return;
    }
    completionHandler(std::nullopt, "b"_s);
}

void IPCTester::stopIfNeeded()
{
    if (RefPtr testQueue = m_testQueue) {
        m_shouldStop = true;
        testQueue->dispatchSync([] { });
        m_testQueue = nullptr;
    }
}

void IPCTester::emptyMessageWithReply(CompletionHandler<void(uint64_t)>&& completionHandler)
{
    completionHandler(0x1);
}

void IPCTester::checkTestParameter(IPC::Connection& connection, TestParameter param)
{
    MESSAGE_CHECK_WITH_MESSAGE_BASE(param.value < 100, connection, "Test parameter must be less than 100");
}

static bool nodesAreEqual(const IPCTesterNode& a, const IPCTesterNode& b)
{
    if (a.name() != b.name() || a.children().size() != b.children().size() || !a.link() != !b.link())
        return false;
    for (size_t i = 0; i < a.children().size(); ++i) {
        if (!nodesAreEqual(a.children()[i], b.children()[i]))
            return false;
    }
    return !a.link() || nodesAreEqual(*a.link(), *b.link());
}

// Sends IPCTesterReceiver in the WebContent process messages that it takes as C++ types and as Swift
// types, and replies with the names of the messages whose replies differ from what IPCTesterPayload.h
// says they should be.
void IPCTester::sendSwiftTypesMessagesToReceiver(IPC::Connection& connection, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto failures = Box<Vector<String>>::create();
    Ref aggregator = CallbackAggregator::create([failures, completionHandler = WTF::move(completionHandler)] mutable {
        completionHandler(makeStringByJoining(failures->span(), ", "_s));
    });
    auto check = [failures](bool passed, ASCIILiteral description) {
        if (!passed)
            failures->append(description);
    };

    auto latin1 = String::fromLatin1("caf\xe9");
    auto utf16 = String::fromUTF8("caf\xc3\xa9 \xe2\x98\xba \xf0\x9f\x98\x80");

    IPCTesterInner inner { -5, "inner"_s };
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TransformInner(inner), [aggregator, check, inner](IPCTesterInner&& reply) {
        check(reply == IPCTesterInner { 5, inner.label }, "TransformInner"_s);
    }, 0);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TransformInnerSwift(inner), [aggregator, check, inner](IPCTesterInner&& reply) {
        check(reply == ipcTesterTransformed(inner), "TransformInnerSwift"_s);
    }, 0);

    Vector<IPCTesterPayload> payloads;
    payloads.append(IPCTesterPayload {
        .flag = true,
        .u8 = 0xFE,
        .u16 = 0xFFFF,
        .u32 = 123456789,
        .u64 = 0x123456789ABCDEF0,
        .i8 = std::numeric_limits<int8_t>::min(),
        .i16 = -2,
        .i32 = std::numeric_limits<int32_t>::max(),
        .i64 = 42,
        .f = 1.5f,
        .d = -0.25,
        .text = "Hello, world"_s,
        .nullText = { },
        .numbers = { 1, 2, 3, 0xFFFFFFFF },
        .strings = { "a"_s, latin1, utf16, emptyString(), String() },
        .color = IPCTesterColor::Blue,
        .flags = { IPCTesterFlag::First, IPCTesterFlag::Third },
        .choice = IPCTesterChoice::Yes,
        .inner = { 7, "inner"_s },
        .inners = { { -1, utf16 }, { 0, String() } },
        .variant = IPCTesterInner { 3, "variant"_s },
        .identifier = IPCConnectionTesterIdentifier::generate(),
    });
    payloads.append(IPCTesterPayload {
        .u64 = std::numeric_limits<uint64_t>::max(),
        .i64 = std::numeric_limits<int64_t>::min(),
        .text = emptyString(),
        .flags = { IPCTesterFlag::First, IPCTesterFlag::Second },
        .variant = std::numeric_limits<uint64_t>::max(),
    });
    payloads.append(IPCTesterPayload {
        .text = latin1,
        .color = IPCTesterColor::Red,
        .flags = { IPCTesterFlag::Second },
        .variant = utf16,
    });
    for (auto& payload : payloads) {
        connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TransformPayloadSwift(payload), [aggregator, check, payload](IPCTesterPayload&& reply) {
            check(reply == ipcTesterTransformed(payload), "TransformPayloadSwift"_s);
        }, 0);
    }

    auto syncResult = connection.sendSync(Messages::IPCTesterReceiver::TransformPayloadSynchronouslySwift(payloads[0]), 0);
    check(syncResult.succeeded() && std::get<0>(syncResult.reply()) == ipcTesterTransformed(payloads[0]), "TransformPayloadSynchronouslySwift"_s);

    Ref node = IPCTesterNode::create("root"_s, { IPCTesterNode::create("first"_s, { }, nullptr), IPCTesterNode::create(String { utf16 }, { }, IPCTesterNode::create("linked"_s, { }, nullptr)) }, nullptr);
    RefPtr link = IPCTesterNode::create("link"_s, { IPCTesterNode::create("child"_s, { }, nullptr) }, nullptr);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TransformNodeSwift(node, link), [aggregator, check, node](RefPtr<IPCTesterNode>&& mirrored, uint32_t nodeCount) {
        check(mirrored && nodesAreEqual(*mirrored, ipcTesterMirrored(node)), "TransformNodeSwift mirrored"_s);
        check(nodeCount == 6, "TransformNodeSwift nodeCount"_s);
    }, 0);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TransformNodeSwift(node, nullptr), [aggregator, check](RefPtr<IPCTesterNode>&& mirrored, uint32_t nodeCount) {
        check(!mirrored, "TransformNodeSwift without link mirrored"_s);
        check(nodeCount == 4, "TransformNodeSwift without link nodeCount"_s);
    }, 0);

    auto identifier = IPCConnectionTesterIdentifier::generate();
    IPCTesterVariant variant = IPCTesterInner { -9, String() };
    auto expectedDescription = ipcTesterDescription(IPCTesterColor::Green, { IPCTesterFlag::First, IPCTesterFlag::Second }, IPCTesterChoice::No, identifier, variant, true);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::DescribeSwift(IPCTesterColor::Green, { IPCTesterFlag::First, IPCTesterFlag::Second }, IPCTesterChoice::No, identifier, variant, true), [aggregator, check, expectedDescription](String&& description) {
        check(description == expectedDescription, "DescribeSwift"_s);
    }, 0);

    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::HalveEvenNumberSwift(10), [aggregator, check](uint32_t half) {
        check(half == 5, "HalveEvenNumberSwift"_s);
    }, 0);

    connection.send(Messages::IPCTesterReceiver::RecordSwift("one"_s), 0);
    connection.send(Messages::IPCTesterReceiver::RecordSwift(String()), 0);
    connection.send(Messages::IPCTesterReceiver::RecordSwift(latin1), 0);
    connection.send(Messages::IPCTesterReceiver::RecordSwift(utf16), 0);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TakeRecordedSwift(), [aggregator, check, latin1, utf16](Vector<String>&& texts) {
        check(texts == Vector<String> { "one"_s, String(), latin1, utf16 } && texts[1].isNull(), "TakeRecordedSwift"_s);
    }, 0);
    connection.send(Messages::IPCTesterReceiver::RecordSwift("two"_s), 0);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::ClearRecordedSwift(), [aggregator] { }, 0);
    connection.sendWithAsyncReply(Messages::IPCTesterReceiver::TakeRecordedSwift(), [aggregator, check](Vector<String>&& texts) {
        check(texts.isEmpty(), "ClearRecordedSwift"_s);
    }, 0);
}

// Connection::addMessageReceiver() refs its receiver on the connection's queue, but IPCTesterReceiver's
// message receiver is only RefCounted, so is registered through this, which forwards on the main thread.
class MainThreadMessageReceiver final : public IPC::MessageReceiver, public ThreadSafeRefCounted<MainThreadMessageReceiver, WTF::DestructionThread::Main> {
public:
    static Ref<MainThreadMessageReceiver> create(Ref<IPC::MessageReceiver>&& receiver)
    {
        return adoptRef(*new MainThreadMessageReceiver(WTF::move(receiver)));
    }

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void didReceiveMessage(IPC::Connection& connection, IPC::Decoder& decoder) final
    {
        assertIsMainThread();
        m_receiver->didReceiveMessage(connection, decoder);
    }

    void didReceiveSyncMessage(IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& replyEncoder) final
    {
        assertIsMainThread();
        m_receiver->didReceiveSyncMessage(connection, decoder, replyEncoder);
    }

private:
    explicit MainThreadMessageReceiver(Ref<IPC::MessageReceiver>&& receiver)
        : m_receiver(WTF::move(receiver))
    {
    }

    const Ref<IPC::MessageReceiver> m_receiver;
};

void IPCTester::addReceiverForSwiftTypesTesting(IPC::Connection& connection, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_threadSafeReceiverForSwiftTypesTesting) {
#if ENABLE(IPC_TESTING_SWIFT)
        m_receiverForSwiftTypesTesting = WTF::makeUniqueWithoutFastMallocCheck<IPCTesterReceiver>(IPCTesterReceiver::init());
        m_threadSafeReceiverForSwiftTypesTesting = MainThreadMessageReceiver::create(m_receiverForSwiftTypesTesting->getMessageReceiver());
#else
        m_receiverForSwiftTypesTesting = IPCTesterReceiver::create();
        m_threadSafeReceiverForSwiftTypesTesting = MainThreadMessageReceiver::create(*m_receiverForSwiftTypesTesting);
#endif
    }
    connection.addMessageReceiver(RunLoop::mainSingleton(), protect(*m_threadSafeReceiverForSwiftTypesTesting), Messages::IPCTesterReceiver::messageReceiverName());
#if ENABLE(IPC_TESTING_SWIFT)
    completionHandler(true);
#else
    completionHandler(false);
#endif
}

void IPCTester::removeReceiverForSwiftTypesTesting(IPC::Connection& connection, CompletionHandler<void()>&& completionHandler)
{
    connection.removeMessageReceiver(Messages::IPCTesterReceiver::messageReceiverName());
    completionHandler();
}

} // namespace WebKit

#endif
