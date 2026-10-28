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

#include "config.h"
#include "PrintProcessConnection.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Decoder.h"
#include "Logging.h"
#include "WebProcess.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PrintProcessConnection);

Ref<PrintProcessConnection> PrintProcessConnection::create(IPC::Connection::Handle&& connectionHandle)
{
    return adoptRef(*new PrintProcessConnection(WTF::move(connectionHandle)));
}

PrintProcessConnection::PrintProcessConnection(IPC::Connection::Handle&& connectionHandle)
    : m_connection(IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(connectionHandle) }))
{
#if ENABLE(IPC_TESTING_API)
    if (RefPtr parentConnection = WebProcess::singleton().parentProcessConnection(); parentConnection && parentConnection->ignoreInvalidMessageForTesting())
        m_connection->setIgnoreInvalidMessageForTesting();
#endif
    m_connection->open(*this);
}

PrintProcessConnection::~PrintProcessConnection()
{
    m_connection->invalidate();
}

void PrintProcessConnection::invalidate()
{
    m_connection->invalidate();
}

void PrintProcessConnection::didClose(IPC::Connection&)
{
    RELEASE_LOG(Process, "%p - PrintProcessConnection::didClose", this);
    WebProcess::singleton().printProcessConnectionDidClose(*this);
}

void PrintProcessConnection::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>&)
{
}

void PrintProcessConnection::didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder)
{
    // The print process sends nothing to the web processes.
    decoder.markInvalid();
}

void PrintProcessConnection::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>&)
{
    decoder.markInvalid();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
