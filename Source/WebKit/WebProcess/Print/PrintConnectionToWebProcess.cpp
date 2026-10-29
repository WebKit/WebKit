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
#include "PrintConnectionToWebProcessMessages.h"
#include "RemotePrintBackend.h"
#include "RemoteSnapshotRegistry.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <wtf/RunLoop.h>
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
    m_printBackends.clear();
    m_connection->invalidate();
}

void PrintConnectionToWebProcess::didClose(IPC::Connection&)
{
    RELEASE_LOG(Process, "%p - PrintConnectionToWebProcess::didClose: webProcessIdentifier=%" PRIu64, this, m_webProcessIdentifier.toUInt64());
    Ref protectedThis { *this };
    m_printBackends.clear();
    WebProcess::singleton().printConnectionToWebProcessDidClose(*this);
}

void PrintConnectionToWebProcess::terminateWebProcess(ProcessIdentifier webProcessIdentifier, IPC::MessageName messageName)
{
    ASSERT(RunLoop::isMain());
    if (RefPtr parentConnection = WebProcess::singleton().parentProcessConnection())
        parentConnection->send(Messages::WebProcessProxy::TerminatePrintProcessClient(webProcessIdentifier, messageName), 0);
}

void PrintConnectionToWebProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>&)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(IPC, "Received an invalid message %s from WebContent process %" PRIu64 ", requesting for it to be terminated.", description(messageName), m_webProcessIdentifier.toUInt64());
    terminateWebProcess(m_webProcessIdentifier, messageName);
}

void PrintConnectionToWebProcess::didReceiveSyncMessage(IPC::Connection&, IPC::Decoder& decoder, UniqueRef<IPC::Encoder>&)
{
    // The web process sends no synchronous messages here.
    decoder.markInvalid();
}

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_connection)

void PrintConnectionToWebProcess::createPrintBackend(RemoteRenderingBackendIdentifier identifier, IPC::StreamServerConnection::Handle&& connectionHandle)
{
    IPC::StreamServerConnectionParameters parameters;
#if ENABLE(IPC_TESTING_API)
    parameters.ignoreInvalidMessageForTesting = m_connection->ignoreInvalidMessageForTesting();
#endif
    RefPtr streamConnection = IPC::StreamServerConnection::tryCreate(WTF::move(connectionHandle), parameters);
    MESSAGE_CHECK(streamConnection);

    auto addResult = m_printBackends.ensure(identifier, [&] {
        return IPC::ScopedActiveMessageReceiveQueue { RemotePrintBackend::create(*this, identifier, streamConnection.releaseNonNull()) };
    });
    if (!addResult.isNewEntry) {
        streamConnection->invalidate();
        MESSAGE_CHECK(false);
    }
}

void PrintConnectionToWebProcess::releasePrintBackend(RemoteRenderingBackendIdentifier identifier)
{
    bool found = m_printBackends.remove(identifier);
    MESSAGE_CHECK(found);
}

void PrintConnectionToWebProcess::abandonSnapshotFrame(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier frameIdentifier)
{
    // FIXME: using global identifiers (frameIdentifier) is not secure. Do not follow this pattern.
    protect(WebProcess::singleton().snapshotRegistry())->abandonSnapshotFrame(snapshotIdentifier, frameIdentifier);
}

void PrintConnectionToWebProcess::failSnapshot(RemoteSnapshotIdentifier snapshotIdentifier)
{
    protect(WebProcess::singleton().snapshotRegistry())->failSnapshot(snapshotIdentifier);
}

#undef MESSAGE_CHECK

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
