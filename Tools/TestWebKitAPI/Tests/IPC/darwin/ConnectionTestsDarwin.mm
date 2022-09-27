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
#include "Connection.h"

#include "ArgumentCoders.h"
#include "Test.h"
#include "Utilities.h"
#include <optional>

namespace TestWebKitAPI {

class ConnectionTestDarwin : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
    }
    ::testing::AssertionResult hasReceiveRight(mach_port_t port)
    {
        mach_port_urefs_t receiveRightCount = 0u;
        auto kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, &receiveRightCount);
        if (kr != KERN_SUCCESS) 
            return ::testing::AssertionFailure() << "got error" << kr;
        if (!receiveRightCount)
            return ::testing::AssertionFailure() << "no receive rights";
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult hasSendRight(mach_port_t port)
    {
        mach_port_urefs_t sendRightCount = 0u;
        auto kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &sendRightCount);
        if (kr != KERN_SUCCESS) 
            return ::testing::AssertionFailure() << "got error" << kr;
        if (!sendRightCount)
            return ::testing::AssertionFailure() << "no send rights";
        return ::testing::AssertionSuccess();
    }
};

TEST_F(ConnectionTestDarwin, HasReceiveRightWorks)
{
    // Proof that tests testing port receive rights have correct logic for detecting
    // receive right destruction.
    mach_port_t port = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    ASSERT_EQ(kr, KERN_SUCCESS);
    ASSERT_TRUE(MACH_PORT_VALID(port));

    EXPECT_TRUE(hasReceiveRight(port));

    // This simulates code that consumes th receive right and destroys it.
    kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
    ASSERT_EQ(kr, KERN_SUCCESS);

    EXPECT_FALSE(hasReceiveRight(port));
}

TEST_F(ConnectionTestDarwin, HasSendRightWorks)
{
    // Proof that tests testing port send rights have correct logic for detecting
    // send right destruction.
    mach_port_t port = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    ASSERT_EQ(kr, KERN_SUCCESS);
    ASSERT_TRUE(MACH_PORT_VALID(port));
    kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    ASSERT_EQ(kr, KERN_SUCCESS);
    EXPECT_TRUE(hasSendRight(port));
    {
        auto sendRight = MachSendRight::adopt(port);
        UNUSED_VARIABLE(sendRight);
        EXPECT_TRUE(hasSendRight(port));
    }
    EXPECT_FALSE(hasSendRight(port));
}

TEST_F(ConnectionTestDarwin, UnopenedServerDestroysPort)
{
    mach_port_t port = MACH_PORT_NULL;
    {
        auto connectionPair = IPC::Connection::createConnectionPair();
        port = connectionPair->server->port();
        EXPECT_TRUE(hasReceiveRight(port));
    }
    EXPECT_FALSE(hasReceiveRight(port));
}

TEST_F(ConnectionTestDarwin, UnopenedClientDestroysPort)
{
    mach_port_t port = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    ASSERT_EQ(kr, KERN_SUCCESS);
    ASSERT_TRUE(MACH_PORT_VALID(port));
    kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    ASSERT_EQ(kr, KERN_SUCCESS);
    {
        Ref<IPC::Connection> client = IPC::Connection::createClientConnection(MachSendRight::adopt(port));
        UNUSED_VARIABLE(client);
        EXPECT_TRUE(hasSendRight(port));
    }
    EXPECT_FALSE(hasSendRight(port));
    EXPECT_TRUE(hasReceiveRight(port));

    kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
    ASSERT_EQ(kr, KERN_SUCCESS);
}

}
