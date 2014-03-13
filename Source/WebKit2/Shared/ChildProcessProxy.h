/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ChildProcessProxy_h
#define ChildProcessProxy_h

#include "Connection.h"
#include "MessageReceiverMap.h"
#include "ProcessLauncher.h"

#include <wtf/ThreadSafeRefCounted.h>

namespace WebKit {

class ChildProcessProxy : ProcessLauncher::Client, public IPC::Connection::Client, public ThreadSafeRefCounted<ChildProcessProxy> {
    WTF_MAKE_NONCOPYABLE(ChildProcessProxy);

public:
    ChildProcessProxy();
    virtual ~ChildProcessProxy();

    // FIXME: This function does an unchecked upcast, and it is only used in a deprecated code path. Would like to get rid of it.
    static ChildProcessProxy* fromConnection(IPC::Connection*);

    void connect();
    void terminate();

    template<typename T> bool send(T&& message, uint64_t destinationID, unsigned messageSendFlags = 0);
    template<typename T> bool sendSync(T&& message, typename T::Reply&&, uint64_t destinationID, std::chrono::milliseconds timeout = std::chrono::seconds(1));
    
    IPC::Connection* connection() const
    {
        ASSERT(m_connection);
        return m_connection.get();
    }

    void addMessageReceiver(IPC::StringReference messageReceiverName, IPC::MessageReceiver&);
    void addMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID, IPC::MessageReceiver&);
    void removeMessageReceiver(IPC::StringReference messageReceiverName, uint64_t destinationID);

    enum class State {
        Launching,
        Running,
        Terminated,
    };
    State state() const;

    PlatformProcessIdentifier processIdentifier() const { return m_processLauncher->processIdentifier(); }

    bool canSendMessage() const { return state() != State::Terminated;}
    bool sendMessage(std::unique_ptr<IPC::MessageEncoder>, unsigned messageSendFlags);

protected:
    void clearConnection();
    void abortProcessLaunchIfNeeded();

    // ProcessLauncher::Client
    virtual void didFinishLaunching(ProcessLauncher*, IPC::Connection::Identifier) override;

    bool dispatchMessage(IPC::Connection*, IPC::MessageDecoder&);
    bool dispatchSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&);

private:
    virtual void getLaunchOptions(ProcessLauncher::LaunchOptions&) = 0;
    virtual void connectionWillOpen(IPC::Connection*);
    virtual void connectionWillClose(IPC::Connection*);

    Vector<std::pair<std::unique_ptr<IPC::MessageEncoder>, unsigned>> m_pendingMessages;
    RefPtr<ProcessLauncher> m_processLauncher;
    RefPtr<IPC::Connection> m_connection;
    IPC::MessageReceiverMap m_messageReceiverMap;
};

template<typename T>
bool ChildProcessProxy::send(T&& message, uint64_t destinationID, unsigned messageSendFlags)
{
    COMPILE_ASSERT(!T::isSync, AsyncMessageExpected);

    auto encoder = std::make_unique<IPC::MessageEncoder>(T::receiverName(), T::name(), destinationID);
    encoder->encode(message.arguments());

    return sendMessage(std::move(encoder), messageSendFlags);
}

template<typename U> 
bool ChildProcessProxy::sendSync(U&& message, typename U::Reply&& reply, uint64_t destinationID, std::chrono::milliseconds timeout)
{
    COMPILE_ASSERT(U::isSync, SyncMessageExpected);

    if (!m_connection)
        return false;

    return connection()->sendSync(std::forward<U>(message), std::move(reply), destinationID, timeout);
}

} // namespace WebKit

#endif // ChildProcessProxy_h
