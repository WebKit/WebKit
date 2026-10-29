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
#include "PrintProcessConnectionBroker.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#include "WebProcessMessages.h"
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>

namespace WebKit {

Ref<PrintProcessConnectionBroker> PrintProcessConnectionBroker::create(bool isLockdownModeEnabled)
{
    return adoptRef(*new PrintProcessConnectionBroker(isLockdownModeEnabled));
}

PrintProcessConnectionBroker::PrintProcessConnectionBroker(bool isLockdownModeEnabled)
    : m_queue(WorkQueue::create("com.apple.WebKit.PrintProcessConnectionBroker"_s))
    , m_isLockdownModeEnabled(isLockdownModeEnabled)
{
}

PrintProcessConnectionBroker::~PrintProcessConnectionBroker() = default;

void PrintProcessConnectionBroker::callOnMainRunLoop(Vector<CompletionHandler<void(bool)>>&& completionHandlers, bool success)
{
    if (completionHandlers.isEmpty())
        return;
    ensureOnMainRunLoop([completionHandlers = WTF::move(completionHandlers), success] mutable {
        for (auto& completionHandler : completionHandlers)
            completionHandler(success);
    });
}

void PrintProcessConnectionBroker::sendToPrintProcess(Function<void(IPC::Connection&)>&& send)
{
    // Messages wait in the pending clients until the print process has launched.
    RefPtr connection = m_printProcessConnection;
    if (!connection)
        return;
    m_queue->dispatch([connection = connection.releaseNonNull(), send = WTF::move(send)] {
        send(connection);
    });
}

void PrintProcessConnectionBroker::sendCreateConnection(WebCore::ProcessIdentifier identifier, PendingClient& pendingClient)
{
    ASSERT(!pendingClient.wasSentToPrintProcess);
    pendingClient.wasSentToPrintProcess = true;
    // The print process creates the pair, since only the client end of one can be sent to another
    // process. The web process is sent the client end once the print process has the server end.
    auto identity = WTF::move(*pendingClient.identity);
    pendingClient.identity = std::nullopt;
    sendToPrintProcess([protectedThis = Ref { *this }, identifier, identity = WTF::move(identity), sharedPreferences = pendingClient.sharedPreferences](IPC::Connection& connection) mutable {
        connection.sendWithAsyncReplyOnDispatcher(Messages::WebProcess::CreatePrintConnectionToWebProcess(identifier, WTF::move(identity), sharedPreferences, protectedThis->m_isLockdownModeEnabled), protectedThis->m_queue.get(), [protectedThis, identifier](std::optional<IPC::Connection::Handle>&& connectionHandle) mutable {
            protectedThis->didCreateConnection(identifier, WTF::move(connectionHandle));
        });
    });
}

void PrintProcessConnectionBroker::connect(Client&& client, CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_lock };
    if (m_isInvalidated) {
        locker.unlockEarly();
        if (completionHandler)
            completionHandler(false);
        return;
    }

    auto identifier = client.identifier;
    if (m_clients.contains(identifier)) {
        // For the print backends that it creates from now on.
        sendToPrintProcess([identifier, sharedPreferences = client.sharedPreferences](IPC::Connection& connection) {
            connection.send(Messages::WebProcess::UpdatePrintConnectionSharedPreferences(identifier, sharedPreferences), 0);
        });
        locker.unlockEarly();
        if (completionHandler)
            completionHandler(true);
        return;
    }

    auto addResult = m_pendingClients.add(identifier, PendingClient { client.connection.ptr(), WTF::move(client.identity), client.sharedPreferences });
    auto& pendingClient = addResult.iterator->value;
    if (completionHandler)
        pendingClient.completionHandlers.append(WTF::move(completionHandler));

    if (!addResult.isNewEntry) {
        if (!pendingClient.wasSentToPrintProcess) {
            pendingClient.sharedPreferences = client.sharedPreferences;
            return;
        }
        // Comes in after the connection is created.
        sendToPrintProcess([identifier, sharedPreferences = client.sharedPreferences](IPC::Connection& connection) {
            connection.send(Messages::WebProcess::UpdatePrintConnectionSharedPreferences(identifier, sharedPreferences), 0);
        });
        return;
    }

    if (m_printProcessConnection)
        sendCreateConnection(identifier, pendingClient);
}

void PrintProcessConnectionBroker::printProcessDidFinishLaunching(IPC::Connection& connection)
{
    ASSERT(RunLoop::isMain());
    Locker locker { m_lock };
    if (m_isInvalidated || m_printProcessConnection)
        return;

    // After the messages that the print process was sent while launching, such as its initialization.
    m_printProcessConnection = &connection;
    for (auto& [identifier, pendingClient] : m_pendingClients) {
        if (!pendingClient.wasSentToPrintProcess)
            sendCreateConnection(identifier, pendingClient);
    }
}

void PrintProcessConnectionBroker::didCreateConnection(WebCore::ProcessIdentifier identifier, std::optional<IPC::Connection::Handle>&& connectionHandle)
{
    assertIsCurrent(m_queue.get());

    std::optional<PendingClient> pendingClient;
    RefPtr<IPC::Connection> printProcessConnection;
    bool success = false;
    {
        Locker locker { m_lock };
        // Failed already, if the web process or the print process went away first.
        pendingClient = m_pendingClients.takeOptional(identifier);
        if (!pendingClient)
            return;
        printProcessConnection = m_printProcessConnection;
        success = !m_isInvalidated && connectionHandle && pendingClient->connection->isValid();
        if (success)
            m_clients.add(identifier);
    }

    if (success) {
        // As DrawFrameToSnapshot is sent, so that it is dispatched while the web process waits for
        // the UI process. A web process asked to record before this comes in waits for it.
        pendingClient->connection->send(Messages::WebProcess::ConnectToPrintProcess(WTF::move(*connectionHandle)), 0, IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    } else if (connectionHandle && printProcessConnection) {
        // The print process would not notice a connection whose client end was never used.
        printProcessConnection->send(Messages::WebProcess::RemovePrintConnectionToWebProcess(identifier), 0);
    }

    callOnMainRunLoop(WTF::move(pendingClient->completionHandlers), success);
}

void PrintProcessConnectionBroker::clientDidDisconnect(WebCore::ProcessIdentifier identifier)
{
    ASSERT(RunLoop::isMain());
    Vector<CompletionHandler<void(bool)>> completionHandlers;
    {
        Locker locker { m_lock };
        bool wasClient = m_clients.remove(identifier);
        auto pendingClient = m_pendingClients.takeOptional(identifier);
        // Behind the request that creates the connection, if it was sent.
        if (wasClient || (pendingClient && pendingClient->wasSentToPrintProcess)) {
            sendToPrintProcess([identifier](IPC::Connection& connection) {
                connection.send(Messages::WebProcess::RemovePrintConnectionToWebProcess(identifier), 0);
            });
        }
        if (pendingClient)
            completionHandlers = WTF::move(pendingClient->completionHandlers);
    }
    for (auto& completionHandler : completionHandlers)
        completionHandler(false);
}

void PrintProcessConnectionBroker::invalidate()
{
    ASSERT(RunLoop::isMain());
    Vector<CompletionHandler<void(bool)>> completionHandlers;
    {
        Locker locker { m_lock };
        m_isInvalidated = true;
        m_printProcessConnection = nullptr;
        // The web processes find out that their connections closed.
        m_clients.clear();
        for (auto& pendingClient : m_pendingClients.values()) {
            for (auto& completionHandler : pendingClient.completionHandlers)
                completionHandlers.append(WTF::move(completionHandler));
        }
        m_pendingClients.clear();
    }
    for (auto& completionHandler : completionHandlers)
        completionHandler(false);
}

bool PrintProcessConnectionBroker::isClient(WebCore::ProcessIdentifier identifier) const
{
    Locker locker { m_lock };
    return m_clients.contains(identifier);
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
