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
#include "SharedPreferencesForWebProcess.h"
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ProcessIdentity.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>

namespace WebKit {

// Connects web processes to one print process. UIKit and WKPrintingView print with the main thread
// blocked, and frame processes may only be connecting then, so nothing that connects a web process
// needs the main thread once the print process has launched: the replies of the print process are
// handled on a queue of the broker's own, which sends the web process its end of the connection.
// Everything is sent to the print process from that queue, so that it comes in order.
class PrintProcessConnectionBroker : public ThreadSafeRefCounted<PrintProcessConnectionBroker> {
public:
    static Ref<PrintProcessConnectionBroker> create(bool isLockdownModeEnabled);
    ~PrintProcessConnectionBroker();

    // A web process to connect, as known on the main thread.
    struct Client {
        WebCore::ProcessIdentifier identifier;
        Ref<IPC::Connection> connection;
        WebCore::ProcessIdentity identity;
        SharedPreferencesForWebProcess sharedPreferences;
    };

    // On the main thread, as are the rest but isClient(). Connects the client, once the print process
    // has launched, unless it is connected or being connected already, which updates the shared
    // preferences of its print backends instead. Calls the completion handler, if any, on the main
    // thread with whether the client was connected.
    void connect(Client&&, CompletionHandler<void(bool)>&& = nullptr);
    void printProcessDidFinishLaunching(IPC::Connection&);
    void clientDidDisconnect(WebCore::ProcessIdentifier);
    // When the print process goes away.
    void invalidate();

    // Whether the client has been sent its end of a connection. Thread-safe.
    bool isClient(WebCore::ProcessIdentifier) const;

private:
    explicit PrintProcessConnectionBroker(bool isLockdownModeEnabled);

    struct PendingClient {
        RefPtr<IPC::Connection> connection;
        std::optional<WebCore::ProcessIdentity> identity;
        SharedPreferencesForWebProcess sharedPreferences;
        bool wasSentToPrintProcess { false };
        Vector<CompletionHandler<void(bool)>> completionHandlers;
    };

    void sendToPrintProcess(Function<void(IPC::Connection&)>&&) WTF_REQUIRES_LOCK(m_lock);
    void sendCreateConnection(WebCore::ProcessIdentifier, PendingClient&) WTF_REQUIRES_LOCK(m_lock);
    void didCreateConnection(WebCore::ProcessIdentifier, std::optional<IPC::Connection::Handle>&&);
    static void callOnMainRunLoop(Vector<CompletionHandler<void(bool)>>&&, bool success);

    const Ref<WorkQueue> m_queue;
    const bool m_isLockdownModeEnabled;
    mutable Lock m_lock;
    RefPtr<IPC::Connection> m_printProcessConnection WTF_GUARDED_BY_LOCK(m_lock);
    bool m_isInvalidated WTF_GUARDED_BY_LOCK(m_lock) { false };
    // The web processes waiting for the print process to launch, or to create their connection.
    HashMap<WebCore::ProcessIdentifier, PendingClient> m_pendingClients WTF_GUARDED_BY_LOCK(m_lock);
    // The web processes that have been sent their end of a connection.
    HashSet<WebCore::ProcessIdentifier> m_clients WTF_GUARDED_BY_LOCK(m_lock);
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
