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

#include "IPCEvent.h"
#include "IPCTestUtilities.h"
#include "Test.h"

namespace TestWebKitAPI {

struct MockTestMessageWithSignal {
    static constexpr bool isSync = false;
    static constexpr bool canDispatchOutOfOrder = false;
    static constexpr bool replyCanDispatchOutOfOrder = false;
    static constexpr IPC::MessageName name()  { return static_cast<IPC::MessageName>(123); }
    auto&& arguments() { return WTFMove(m_arguments); }
    MockTestMessageWithSignal(IPC::Signal&& signal)
        : m_arguments(WTFMove(signal))
    {
    }
    std::tuple<IPC::Signal&&> m_arguments;
};

class EventTestABBA : public testing::TestWithParam<std::tuple<ConnectionTestDirection>>, protected ConnectionTestBase {
public:
    bool serverIsA() const { return std::get<0>(GetParam()) == ConnectionTestDirection::ServerIsA; }

    void SetUp() override
    {
        setupBase();
        if (!serverIsA())
            std::swap(m_connections[0].connection, m_connections[1].connection);
    }

    void TearDown() override
    {
        teardownBase();
    }

    Ref<RunLoop> createRunLoop(ASCIILiteral name)
    {
        auto runLoop = RunLoop::create(name, ThreadType::Unknown);
        m_runLoops.append(runLoop);
        return runLoop;
    }

protected:
    Vector<Ref<RunLoop>> m_runLoops;
};

#define LOCAL_STRINGIFY(x) #x
#define RUN_LOOP_NAME "RunLoop at EventTests.cpp:" LOCAL_STRINGIFY(__LINE__) ""_s

TEST_P(EventTestABBA, SerializeAndSignal)
{
    ASSERT_TRUE(openA());

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());

        bClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
            auto signal = decoder.decode<IPC::Signal>();
            signal->signal();

            b()->invalidate();
            return true;
        });
    });

    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair);
    a()->send(MockTestMessageWithSignal { WTFMove(pair->signal) }, 77);

    pair->event.wait();
}

TEST_P(EventTestABBA, InterruptOnDestruct)
{
    ASSERT_TRUE(openA());

    auto runLoop = createRunLoop(RUN_LOOP_NAME);
    runLoop->dispatch([&] {
        ASSERT_TRUE(openB());

        bClient().setAsyncMessageHandler([&] (IPC::Decoder& decoder) -> bool {
            {
                auto signal = decoder.decode<IPC::Signal>();
            }

            b()->invalidate();
            return true;
        });
    });

    auto pair = IPC::createEventSignalPair();
    ASSERT_TRUE(pair);
    a()->send(MockTestMessageWithSignal { WTFMove(pair->signal) }, 77);

    pair->event.wait();
}

#undef RUN_LOOP_NAME
#undef LOCAL_STRINGIFY

INSTANTIATE_TEST_SUITE_P(EventTest,
    EventTestABBA,
    testing::Values(ConnectionTestDirection::ServerIsA, ConnectionTestDirection::ClientIsA),
    TestParametersToStringFormatter());


}
