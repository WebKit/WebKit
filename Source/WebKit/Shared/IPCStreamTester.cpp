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

#include "config.h"
#include "IPCStreamTester.h"

#if ENABLE(IPC_TESTING_API)
#include "Decoder.h"
#include "IPCStreamTesterMessages.h"
#include "IPCStreamTesterProxyMessages.h"
#include "IPCUtilities.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamServerConnection.h"

#if USE(FOUNDATION)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WebKit {

RefPtr<IPCStreamTester> IPCStreamTester::create(IPC::Connection& connection, IPCStreamTesterIdentifier identifier, IPC::StreamConnectionBuffer&& stream)
{
    auto tester = adoptRef(*new IPCStreamTester(connection, identifier, WTFMove(stream)));
    tester->initialize();
    return tester;
}

IPCStreamTester::IPCStreamTester(IPC::Connection& connection, IPCStreamTesterIdentifier identifier, IPC::StreamConnectionBuffer&& stream)
    : m_workQueue(IPC::StreamConnectionWorkQueue::create("IPCStreamTester work queue"))
    , m_streamConnection(IPC::StreamServerConnection::create(connection, WTFMove(stream), workQueue()))
    , m_identifier(identifier)
{
}

IPCStreamTester::~IPCStreamTester() = default;

void IPCStreamTester::initialize()
{
    m_streamConnection->startReceivingMessages(*this, Messages::IPCStreamTester::messageReceiverName(), m_identifier.toUInt64());
    m_streamConnection->open();
    workQueue().dispatch([this] {
        m_streamConnection->send(Messages::IPCStreamTesterProxy::WasCreated(workQueue().wakeUpSemaphore(), m_streamConnection->clientWaitSemaphore()), m_identifier);
    });
}

void IPCStreamTester::stopListeningForIPC(Ref<IPCStreamTester>&& refFromConnection)
{
    m_streamConnection->invalidate();
    m_streamConnection->stopReceivingMessages(Messages::IPCStreamTester::messageReceiverName(), m_identifier.toUInt64());
    workQueue().stopAndWaitForCompletion();
}

void IPCStreamTester::syncMessageReturningSharedMemory1(uint32_t byteCount, CompletionHandler<void(SharedMemory::Handle)>&& completionHandler)
{
    auto result = [&]() -> SharedMemory::Handle {
        auto sharedMemory = WebKit::SharedMemory::allocate(byteCount);
        if (!sharedMemory)
            return { };
        auto handle = sharedMemory->createHandle(SharedMemory::Protection::ReadOnly);
        if (!handle)
            return { };
        uint8_t* data = static_cast<uint8_t*>(sharedMemory->data());
        for (size_t i = 0; i < sharedMemory->size(); ++i)
            data[i] = i;
        return *handle;
    }();
    completionHandler(WTFMove(result));
}

void IPCStreamTester::syncCrashOnZero(int32_t value, CompletionHandler<void(int32_t)>&& completionHandler)
{
    if (!value) {
        // Use exit so that we don't leave a crash report.
#if OS(WINDOWS)
        // Calling _exit in non-main threads may cause a deadlock in WTF::Thread::ThreadHolder::~ThreadHolder.
        TerminateProcess(GetCurrentProcess(), EXIT_SUCCESS);
#else
        _exit(EXIT_SUCCESS);
#endif
    }
    completionHandler(value);
}

#if USE(FOUNDATION)

namespace {
struct UseCountHolder {
    std::shared_ptr<bool> value;
};
}

static void releaseUseCountHolder(CFAllocatorRef, const void* value)
{
    delete static_cast<const UseCountHolder*>(value);
}

#endif

void IPCStreamTester::checkAutoreleasePool(CompletionHandler<void(int32_t)>&& completionHandler)
{
    if (!m_autoreleasePoolCheckValue)
        m_autoreleasePoolCheckValue = std::make_shared<bool>(true);
    completionHandler(m_autoreleasePoolCheckValue.use_count());

#if USE(FOUNDATION)
    static const CFArrayCallBacks arrayCallbacks {
        .release = releaseUseCountHolder,
    };
    const void* values[] = { new UseCountHolder { m_autoreleasePoolCheckValue } };
    CFArrayRef releaseDetector = CFArrayCreate(kCFAllocatorDefault, values, 1, &arrayCallbacks);
    CFAutorelease(releaseDetector);
#endif
}
}

#endif
