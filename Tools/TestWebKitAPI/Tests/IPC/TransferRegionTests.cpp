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

#include "IPCTestUtilities.h"
#include "Test.h"
#include <WebCore/ProcessIdentity.h>
#include <wtf/StdLibExtras.h>
#include <wtf/threads/BinarySemaphore.h>

#define RUN_LOOP_NAME "RunLoop at TransferRegionTests.cpp:" STRINGIZE_VALUE_OF(__LINE__)
namespace TestWebKitAPI {

static constexpr Seconds kDefaultWaitForTimeout = 1_s;

class MockSendTransferRegion {
public:
    using Arguments = std::tuple<IPC::TransferRegion>;
    static IPC::MessageName name() { return IPC::MessageName::IPCTester_SendTransferRegion; }
    static constexpr bool isSync = true;
    static constexpr auto callbackThread = WTF::CompletionHandlerCallThread::ConstructionThread;
    using ReplyArguments = std::tuple<>;
    explicit MockSendTransferRegion(const IPC::TransferRegion& region)
        : m_arguments(region)
    {
    }
    const auto& arguments() const
    {
        return m_arguments;
    }
private:
    std::tuple<const IPC::TransferRegion&> m_arguments;
};

class TransferRegionTest : public ConnectionTestABBA {
};

static size_t testBufferSize(size_t iteration)
{
    if (iteration < 5)
        return static_cast<size_t>(iteration * 1.5f);
    size_t rest = iteration % 2 ? -iteration : iteration;
    if (iteration < 20)
        return iteration * 2000 + rest;
    if (iteration < 30)
        return iteration * 30000 + rest;
    if (iteration < 40)
        return iteration * 500000 + rest;
    return iteration * 7000000 + rest;
}

TEST_P(TransferRegionTest, SendSyncTransferRegion)
{
    auto cleanup = makeLocalReferenceBarrier();

    ASSERT_TRUE(openA());
    WebCore::ProcessIdentity resourceOwner;
    bClient().setSyncMessageHandler([&] (IPC::Connection& connection, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>& encoder) -> bool {
        auto region = decoder.decode<IPC::TransferRegion>();
        if (!region)
            return false;
        auto mappedRegion = connection.mapTransferRegion(WTFMove(*region), resourceOwner);
        if (!mappedRegion)
            return false;
        uint8_t value = static_cast<uint8_t>(decoder.destinationID());
        for (auto& d : mappedRegion->data())
            d = value++;
        connection.sendSyncReply(WTFMove(encoder));
        return true;
    });
    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    BinarySemaphore semaphore;
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());
        bClient().waitForDidClose(kDefaultWaitForTimeout);
        semaphore.signal();
    });
    for (uint64_t i = 0u; i < 1u; ++i) {
        auto buffer = a()->reserveTransferRegion(testBufferSize(i));
        ASSERT_TRUE(!!buffer);
        auto data = buffer->data();
        for (size_t j = 0u; j < 1; ++j) {
            memset(data.data(), 0xee, data.size());
            auto result = a()->sendSync(MockSendTransferRegion { buffer->passRegion() }, i);
            ASSERT_TRUE(!!result);
            uint8_t value = static_cast<uint8_t>(i);
            for (size_t k = 0; k < data.size(); ++k)
                EXPECT_EQ(value++, data[k]) << j << i;
        }
    }
    a()->invalidate();
    EXPECT_TRUE(semaphore.waitFor(kDefaultWaitForTimeout));
}

#undef RUN_LOOP_NAME

INSTANTIATE_TEST_SUITE_P(ConnectionTest,
    TransferRegionTest,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());

}
