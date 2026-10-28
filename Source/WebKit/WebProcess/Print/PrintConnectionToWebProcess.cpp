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
#include "PrintConnectionToWebProcess.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Decoder.h"
#include "Logging.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PrintConnectionToWebProcess);

Ref<PrintConnectionToWebProcess> PrintConnectionToWebProcess::create(ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier&& connectionIdentifier, ProcessIdentity&& webProcessIdentity, SharedPreferencesForWebProcess&& sharedPreferencesForWebProcess, bool isLockdownModeEnabled)
{
    return adoptRef(*new PrintConnectionToWebProcess(webProcessIdentifier, WTF::move(connectionIdentifier), WTF::move(webProcessIdentity), WTF::move(sharedPreferencesForWebProcess), isLockdownModeEnabled));
}

PrintConnectionToWebProcess::PrintConnectionToWebProcess(ProcessIdentifier webProcessIdentifier, IPC::Connection::Identifier&& connectionIdentifier, ProcessIdentity&& webProcessIdentity, SharedPreferencesForWebProcess&& sharedPreferencesForWebProcess, bool isLockdownModeEnabled)
    : m_connection(IPC::Connection::createServerConnection(WTF::move(connectionIdentifier)))
    , m_webProcessIdentifier(webProcessIdentifier)
    , m_webProcessIdentity(WTF::move(webProcessIdentity))
    , m_sharedPreferencesForWebProcess(WTF::move(sharedPreferencesForWebProcess))
    , m_isLockdownModeEnabled(isLockdownModeEnabled)
{
    m_connection->open(*this);
}

PrintConnectionToWebProcess::~PrintConnectionToWebProcess()
{
    m_connection->invalidate();
}

void PrintConnectionToWebProcess::invalidate()
{
    m_connection->invalidate();
}

void PrintConnectionToWebProcess::didClose(IPC::Connection&)
{
    RELEASE_LOG(Process, "%p - PrintConnectionToWebProcess::didClose: webProcessIdentifier=%" PRIu64, this, m_webProcessIdentifier.toUInt64());
    WebProcess::singleton().printConnectionToWebProcessDidClose(*this);
}

void PrintConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, "Received an invalid message %s from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName), m_webProcessIdentifier.toUInt64());
    if (RefPtr parentConnection = WebProcess::singleton().parentProcessConnection())
        parentConnection->send(Messages::WebProcessProxy::TerminatePrintProcessClient(m_webProcessIdentifier, messageName), 0);
}

void PrintConnectionToWebProcess::didReceiveMessage(IPC::Connection&, IPC::Decoder& decoder)
{
    // The web process has nothing to send here yet.
    decoder.markInvalid();
}

void PrintConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>&)
{
    decoder.markInvalid();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
