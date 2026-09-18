/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

// A receiver which cannot answer a message until a later one arrives, over a real connection.
//
// Message A asks for a reply the receiver has not got yet, so it keeps the reply and returns. Message
// B lets it answer. A's reply therefore arrives only after B was handled, which for the Swift
// receiver also means only after the main actor has had a turn - the part nothing in WebKit obviously
// provides. Both receivers answer through a refcounted WTF::CompletionHandler, as a generated
// receiver is handed one, and the test waits with Util::waitFor so it spins WTF's run loop the way
// WebKit's main thread does.
//
// The C++ receiver is here to show what the Swift one has to match.

#include "config.h"
#include "Helpers/Utilities.h"
#include "IPCTestUtilities.h"
#include "SwiftDeferredReplySupport.h"

// cmake puts the Swift in the TestIPCLibrary helper library; Xcode puts it in TestIPC itself. Either
// way the module name becomes the namespace of the generated header.
#if defined(BUILDING_WITH_CMAKE)
#include <TestIPCLibrary-Swift.h>
namespace SwiftReceiver = TestIPCLibrary;
#else
#include "TestIPC-Swift.h"
namespace SwiftReceiver = TestIPC;
#endif

namespace TestWebKitAPI {

class DeferredReplyTest : public testing::Test, protected ConnectionTestBase {
public:
    void SetUp() override
    {
        setupBase();
        resetCxxDeferredReplyReceiver();
        forgetDeferredRepliesForSwift();
        SwiftReceiver::testIPCResetDeferredReplyState();
    }

    void TearDown() override
    {
        teardownBase();
    }

protected:
    // Hands each incoming message to a receiver, wrapping message A's reply the way
    // IPC::handleMessageAsync does: a WTF::CompletionHandler which outlives this dispatch.
    template<typename DeferFunction, typename ReleaseFunction>
    void routeMessages(DeferFunction&& defer, ReleaseFunction&& release)
    {
        aClient().setAsyncMessageHandler([defer = std::forward<DeferFunction>(defer), release = std::forward<ReleaseFunction>(release)] (IPC::Connection& connection, IPC::Decoder& decoder) mutable -> bool {
            if (decoder.messageName() == MockTestMessageWithAsyncReply1::name()) {
                auto listenerID = decoder.decode<uint64_t>();
                if (!listenerID)
                    return false;
                auto reply = DeferredReply::create(CompletionHandler<void(uint64_t)> { [protectedConnection = Ref { connection }, listenerID = *listenerID] (uint64_t value) mutable {
                    auto encoder = makeUniqueRef<IPC::Encoder>(MockTestMessageWithAsyncReply1::asyncMessageReplyName(), listenerID);
                    encoder.get() << value;
                    protectedConnection->sendSyncReply(WTF::move(encoder));
                } });
                defer(reply.get());
                return true;
            }
            if (decoder.messageName() == MockTestMessage1::name()) {
                release();
                return true;
            }
            return false;
        });
    }
};

TEST_F(DeferredReplyTest, CxxHandlerAnswersOnceALaterMessageArrives)
{
    ASSERT_TRUE(openBoth());
    auto& receiver = cxxDeferredReplyReceiver();
    routeMessages([&] (DeferredReply& reply) {
        receiver.handleMessageDeferringItsReply(reply);
    }, [&] {
        receiver.handleMessageReleasingTheDeferredReply();
    });

    std::optional<uint64_t> reply;
    b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&] (uint64_t value) {
        reply = value;
    }, 0);

    EXPECT_TRUE(Util::waitFor([&] {
        return receiver.handlerStartedOnMainThread();
    }));
    EXPECT_FALSE(receiver.handlerDidResume());
    EXPECT_FALSE(reply.has_value());

    b()->send(MockTestMessage1 { }, 0);

    EXPECT_TRUE(Util::waitFor([&] {
        return reply.has_value();
    }));
    EXPECT_EQ(reply.value_or(0), deferredReplyValue);
    EXPECT_TRUE(receiver.handlerResumedOnMainThread());

    b()->invalidate();
    a()->invalidate();
}

TEST_F(DeferredReplyTest, SwiftHandlerAnswersOnceALaterMessageArrives)
{
    ASSERT_TRUE(openBoth());
    routeMessages([] (DeferredReply& reply) {
        SwiftReceiver::testIPCDispatchMessageDeferringItsReply(registerDeferredReplyForSwift(reply));
    }, [] {
        SwiftReceiver::testIPCDispatchMessageReleasingTheDeferredReply();
    });

    std::optional<uint64_t> reply;
    b()->sendWithAsyncReply(MockTestMessageWithAsyncReply1 { }, [&] (uint64_t value) {
        reply = value;
    }, 0);

    // Task.immediate runs the handler up to its suspension during dispatch, so it has started but
    // cannot have answered: it is waiting for message B.
    EXPECT_TRUE(Util::waitFor([&] {
        return SwiftReceiver::testIPCHandlerStartedOnMainThread();
    }));
    EXPECT_FALSE(SwiftReceiver::testIPCHandlerDidResume());
    EXPECT_FALSE(reply.has_value());

    b()->send(MockTestMessage1 { }, 0);

    // Resuming a continuation enqueues a main actor job rather than running it inline, so the
    // handler's remainder needs a turn of the run loop. If it never gets one, a deferred reply never
    // arrives in production either.
    EXPECT_TRUE(Util::waitFor([&] {
        return reply.has_value();
    }));
    EXPECT_EQ(reply.value_or(0), deferredReplyValue);
    EXPECT_TRUE(SwiftReceiver::testIPCHandlerDidResume());
    EXPECT_TRUE(SwiftReceiver::testIPCHandlerResumedOnMainThread());

    b()->invalidate();
    a()->invalidate();
}

} // namespace TestWebKitAPI
