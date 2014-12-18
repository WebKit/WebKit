/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "MachSendRight.h"

#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <utility>

namespace WebCore {

static void retainSendRight(mach_port_t port)
{
    if (!MACH_PORT_VALID(port))
        return;

    auto kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, 1);
    if (kr != KERN_SUCCESS)
        LOG_ERROR("mach_port_mod_refs error: %s (%x)", mach_error_string(kr), kr);
}

static void releaseSendRight(mach_port_t port)
{
    if (!MACH_PORT_VALID(port))
        return;

    auto kr = mach_port_deallocate(mach_task_self(), port);
    if (kr != KERN_SUCCESS)
        LOG_ERROR("mach_port_deallocate error: %s (%x)", mach_error_string(kr), kr);
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
