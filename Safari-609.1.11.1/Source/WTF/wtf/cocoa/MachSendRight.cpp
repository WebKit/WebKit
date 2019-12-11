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

#define LOG_CHANNEL_PREFIX Log

namespace WTF {

#if RELEASE_LOG_DISABLED
WTFLogChannel LogProcess = { WTFLogChannelState::On, "Process", WTFLogLevel::Error };
#else
WTFLogChannel LogProcess = { WTFLogChannelState::On, "Process", WTFLogLevel::Error, LOG_CHANNEL_WEBKIT_SUBSYSTEM, OS_LOG_DEFAULT };
#endif

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

static void releaseSendRight(mach_port_t port)
{
    if (port == MACH_PORT_NULL)
        return;

    deallocateSendRightSafely(port);
}

void deallocateSendRightSafely(mach_port_t port)
{
    auto kr = mach_port_deallocate(mach_task_self(), port);
    if (kr == KERN_SUCCESS)
        return;

    RELEASE_LOG_ERROR(Process, "mach_port_deallocate error for port %d: %{private}s (%#x)", port, mach_error_string(kr), kr);
    if (kr == KERN_INVALID_RIGHT || kr == KERN_INVALID_NAME)
        CRASH();
}

MachSendRight MachSendRight::adopt(mach_port_t port)
{
    return MachSendRight(port);
}

MachSendRight MachSendRight::create(mach_port_t port)
{
    retainSendRight(port);

    return adopt(port);
}

MachSendRight::MachSendRight(mach_port_t port)
    : m_port(port)
{
}

MachSendRight::MachSendRight(MachSendRight&& other)
    : m_port(other.leakSendRight())
{
}

MachSendRight::~MachSendRight()
{
    releaseSendRight(m_port);
}

MachSendRight& MachSendRight::operator=(MachSendRight&& other)
{
    if (this != &other) {
        releaseSendRight(m_port);
        m_port = other.leakSendRight();
    }

    return *this;
}

MachSendRight MachSendRight::copySendRight() const
{
    return create(m_port);
}

mach_port_t MachSendRight::leakSendRight()
{
    return std::exchange(m_port, MACH_PORT_NULL);
}

}
