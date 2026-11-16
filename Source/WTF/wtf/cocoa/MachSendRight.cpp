/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <utility>

#include <wtf/Logging.h>

namespace WTF {

static void retainSendRight(mach_port_t port)
{
    if (port == MACH_PORT_NULL)
        return;

    auto kr = KERN_SUCCESS;
    if (port != MACH_PORT_DEAD)
        kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);

    if (kr == KERN_INVALID_RIGHT || port == MACH_PORT_DEAD)
        kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, 1);

    if (kr != KERN_SUCCESS) {
        LOG_ERROR("mach_port_mod_refs error for port %d: %s (%x)", port, mach_error_string(kr), kr);
        if (kr == KERN_INVALID_RIGHT)
            CRASH();
    }
}

void deallocateSendRightSafely(mach_port_t port)
{
    if (port == MACH_PORT_NULL)
        return;

    auto kr = mach_port_deallocate(mach_task_self(), port);
    if (kr == KERN_SUCCESS)
        return;

    RELEASE_LOG_ERROR(Process, "mach_port_deallocate error for port %d: %{private}s (%#x)", port, mach_error_string(kr), kr);
    if (kr == KERN_INVALID_RIGHT || kr == KERN_INVALID_NAME)
        CRASH();
}

static void assertSendRight(mach_port_t port)
{
    if (port == MACH_PORT_NULL)
        return;

    unsigned count = 0;
    auto kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &count);
    if (kr == KERN_SUCCESS && !count)
        kr = mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_DEAD_NAME, &count);

    if (kr == KERN_SUCCESS && count > 0)
        return;

    RELEASE_LOG_ERROR(Process, "mach_port_get_refs error for port %d: %{private}s (%#x)", port, mach_error_string(kr), kr);
    CRASH();
}

MachSendRight MachSendRight::adopt(mach_port_t port)
{
    assertSendRight(port);
    return MachSendRight(port);
}

MachSendRight MachSendRight::create(mach_port_t port)
{
    retainSendRight(port);
    return adopt(port);
}

MachSendRight MachSendRight::createFromReceiveRight(mach_port_t receiveRight)
{
    ASSERT(MACH_PORT_VALID(receiveRight));
    if (mach_port_insert_right(mach_task_self(), receiveRight, receiveRight, MACH_MSG_TYPE_MAKE_SEND) == KERN_SUCCESS)
        return MachSendRight { receiveRight };
    return { };
}

MachSendRight::MachSendRight(mach_port_t port)
    : m_port(port)
{
}

MachSendRight::MachSendRight(MachSendRight&& other)
    : m_port(other.leakSendRight())
{
}

MachSendRight::MachSendRight(const MachSendRight& other)
    : m_port(other.m_port)
{
    retainSendRight(m_port);
}

MachSendRight::~MachSendRight()
{
    deallocateSendRightSafely(m_port);
}

MachSendRight& MachSendRight::operator=(MachSendRight&& other)
{
    if (this != &other) {
        deallocateSendRightSafely(m_port);
        m_port = other.leakSendRight();
    }

    return *this;
}

mach_port_t MachSendRight::leakSendRight()
{
    return std::exchange(m_port, MACH_PORT_NULL);
}

}
