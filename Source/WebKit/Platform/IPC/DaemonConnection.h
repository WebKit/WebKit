/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#include <wtf/CompletionHandler.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/CString.h>

#if PLATFORM(COCOA)
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#endif

namespace WebKit {

namespace Daemon {

using EncodedMessage = Vector<uint8_t>;

class Connection : public RefCountedAndCanMakeWeakPtr<Connection> {
public:
#if PLATFORM(COCOA)
    static Ref<Connection> create(OSObjectPtr<xpc_connection_t>&& connection)
    {
        return adoptRef(*new Connection(WTFMove(connection)));
    }
#endif

    virtual ~Connection() = default;

#if PLATFORM(COCOA)
    xpc_connection_t get() const { return m_connection.get(); }
    void send(xpc_object_t) const;
    void sendWithReply(xpc_object_t, CompletionHandler<void(xpc_object_t)>&&) const;
#endif

protected:
    Connection() = default;

#if PLATFORM(COCOA)
    explicit Connection(OSObjectPtr<xpc_connection_t>&& connection)
        : m_connection(WTFMove(connection)) { }
#endif

    virtual void initializeConnectionIfNeeded() const { }

#if PLATFORM(COCOA)
    mutable OSObjectPtr<xpc_connection_t> m_connection;
#endif
};

template<typename Traits>
class ConnectionToMachService : public Connection {
public:
    virtual ~ConnectionToMachService() = default;

    void send(typename Traits::MessageType, EncodedMessage&&) const;
    void sendWithReply(typename Traits::MessageType, EncodedMessage&&, CompletionHandler<void(EncodedMessage&&)>&&) const;

    virtual void newConnectionWasInitialized() const = 0;

#if PLATFORM(COCOA)
    virtual OSObjectPtr<xpc_object_t> dictionaryFromMessage(typename Traits::MessageType, EncodedMessage&&) const = 0;
    virtual void connectionReceivedEvent(xpc_object_t) = 0;
#endif

    const CString& machServiceName() const { return m_machServiceName; }

protected:
    explicit ConnectionToMachService(CString&& machServiceName)
        : m_machServiceName(WTFMove(machServiceName))
    { }

private:
    void initializeConnectionIfNeeded() const final;

    const CString m_machServiceName;
};

} // namespace Daemon

} // namespace WebKit
