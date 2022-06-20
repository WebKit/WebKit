/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#include <wtf/MachSendRight.h>

#include <mach/mach.h>

namespace TestWebKitAPI {

static unsigned getSendRefs(mach_port_t port)
{
    unsigned count = 0;
    auto kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &count);
    EXPECT_EQ(kr, KERN_SUCCESS);
    return count;
}

static mach_port_t setup()
{
    mach_port_t port = MACH_PORT_NULL;
    auto kr = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    EXPECT_EQ(kr, KERN_SUCCESS);

    kr = mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND);
    EXPECT_EQ(kr, KERN_SUCCESS);
    
    EXPECT_EQ(getSendRefs(port), 1u);

    return port;
}

void shutdown(mach_port_t port)
{
    EXPECT_EQ(getSendRefs(port), 0u);
    EXPECT_EQ(mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1), KERN_SUCCESS);

    // Confirm that releasing the receive right reference resulted in the port name being released.
    EXPECT_EQ(mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1), KERN_INVALID_NAME);
}

TEST(MachSendRight, Adopt)
{
    auto port = setup();

    {
        MachSendRight right = MachSendRight::adopt(port);
        EXPECT_EQ(getSendRefs(port), 1u);
    }

    shutdown(port);
}

TEST(MachSendRight, Copy)
{
    auto port = setup();

    {
        MachSendRight right = MachSendRight::adopt(port);
        MachSendRight copy = right;

        EXPECT_EQ(getSendRefs(port), 2u);
    }

    shutdown(port);
}

TEST(MachSendRight, Move)
{
    auto port = setup();

    {
        MachSendRight right = MachSendRight::adopt(port);
        MachSendRight move = WTFMove(right);

        EXPECT_EQ(getSendRefs(port), 1u);
    }

    shutdown(port);
}

TEST(MachSendRight, Overwrite)
{
    auto first = setup();
    auto second = setup();

    {
        MachSendRight firstRight = MachSendRight::adopt(first);
        MachSendRight secondRight = MachSendRight::adopt(second);

        secondRight = firstRight;

        EXPECT_EQ(getSendRefs(first), 2u);
        EXPECT_EQ(getSendRefs(second), 0u);
    }

    shutdown(first);
    shutdown(second);
}

TEST(MachSendRight, OverwriteMove)
{
    auto first = setup();
    auto second = setup();

    {
        MachSendRight firstRight = MachSendRight::adopt(first);
        MachSendRight secondRight = MachSendRight::adopt(second);

        secondRight = WTFMove(firstRight);

        EXPECT_EQ(getSendRefs(first), 1u);
        EXPECT_EQ(getSendRefs(second), 0u);
    }

    shutdown(first);
    shutdown(second);
}


TEST(MachSendRight, DeadName)
{
    // Release the receive right so that the send right becomes a dead name right instead, and
    // confirm that MachSendRight handles this.
    auto port = setup();
    EXPECT_EQ(mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1), KERN_SUCCESS);

    {
        MachSendRight right = MachSendRight::adopt(port);
        EXPECT_EQ(getSendRefs(port), 0u);
        
        unsigned count = 0;
        auto kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, &count);
        EXPECT_EQ(kr, KERN_SUCCESS);
        EXPECT_EQ(count, 1u);
    }
    
    EXPECT_EQ(mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, -1), KERN_INVALID_NAME);
}

}


