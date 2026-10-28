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

#pragma once

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "Connection.h"
#include "RemoteRenderingBackendIdentifier.h"
#include "RemoteSnapshotIdentifier.h"
#include "ScopedActiveMessageReceiveQueue.h"
#include "SharedPreferencesForWebProcess.h"
#include "StreamServerConnection.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

class RemotePrintBackend;

// In the print process, the connection from a web process that draws frames into print snapshots.
// The UI process creates it, so that the web process gets its end before it is asked to draw.
class PrintConnectionToWebProcess final : public ThreadSafeRefCounted<PrintConnectionToWebProcess, WTF::DestructionThread::Main>, public IPC::Connection::Client {
    WTF_MAKE_TZONE_ALLOCATED(PrintConnectionToWebProcess);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PrintConnectionToWebProcess);
public:
    static Ref<PrintConnectionToWebProcess> create(WebCore::ProcessIdentifier, IPC::Connection::Identifier&&, WebCore::ProcessIdentity&&, SharedPreferencesForWebProcess&&, bool isLockdownModeEnabled);
    ~PrintConnectionToWebProcess();

    void ref() const final { ThreadSafeRefCounted::ref(); }
    void deref() const final { ThreadSafeRefCounted::deref(); }

    void invalidate();

    WebCore::ProcessIdentifier webProcessIdentifier() const { return m_webProcessIdentifier; }
    const WebCore::ProcessIdentity& webProcessIdentity() const LIFETIME_BOUND { return m_webProcessIdentity; }
    // Taken by the print backends created after it is updated.
    const SharedPreferencesForWebProcess& sharedPreferencesForWebProcessValue() const LIFETIME_BOUND { return m_sharedPreferencesForWebProcess; }
    void updateSharedPreferencesForWebProcess(SharedPreferencesForWebProcess&& sharedPreferencesForWebProcess) { m_sharedPreferencesForWebProcess = WTF::move(sharedPreferencesForWebProcess); }
    bool isLockdownModeEnabled() const { return m_isLockdownModeEnabled; }

    // Asks the UI process to terminate the web process, for an invalid message. Called on the main thread.
    static void terminateWebProcess(WebCore::ProcessIdentifier, IPC::MessageName);

private:
    PrintConnectionToWebProcess(WebCore::ProcessIdentifier, IPC::Connection::Identifier&&, WebCore::ProcessIdentity&&, SharedPreferencesForWebProcess&&, bool isLockdownModeEnabled);

    // IPC::Connection::Client
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding) final;
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    // Messages.
    void createPrintBackend(RemoteRenderingBackendIdentifier, IPC::StreamServerConnection::Handle&&);
    void releasePrintBackend(RemoteRenderingBackendIdentifier);
    void abandonSnapshotFrame(RemoteSnapshotIdentifier, WebCore::FrameIdentifier);
    void failSnapshot(RemoteSnapshotIdentifier);

    const Ref<IPC::Connection> m_connection;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
    const WebCore::ProcessIdentity m_webProcessIdentity;
    SharedPreferencesForWebProcess m_sharedPreferencesForWebProcess;
    const bool m_isLockdownModeEnabled;
    HashMap<RemoteRenderingBackendIdentifier, IPC::ScopedActiveMessageReceiveQueue<RemotePrintBackend>> m_printBackends;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
