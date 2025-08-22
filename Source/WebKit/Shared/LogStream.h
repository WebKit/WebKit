/* Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#include "LogStreamIdentifier.h"
#include <wtf/ProcessID.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#else
#include "MessageReceiver.h"
#include <wtf/RefCounted.h>
#endif

namespace WebKit {

constexpr size_t logCategoryMaxSize = 32;
constexpr size_t logSubsystemMaxSize = 32;
constexpr size_t logStringMaxSize = 256;

class WebProcessProxy;
// Type which receives log messages from another process and invokes the platform logging.
// The messages are found from generated LogStream.messages.in in build directory,
// DerivedSources/WebKit/LogStream.messages.in.
class LogStream final :
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    public IPC::StreamServerConnection::Client
#else
    public RefCounted<LogStream>, public IPC::MessageReceiver
#endif
{
    WTF_MAKE_TZONE_ALLOCATED(LogStream);
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(LogStream);
#endif
public:
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    static RefPtr<LogStream> create(WebProcessProxy&, IPC::StreamServerConnectionHandle&&, LogStreamIdentifier, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&&);
#else
    static Ref<LogStream> create(WebProcessProxy&, Ref<IPC::Connection>&&, LogStreamIdentifier);
#endif
    ~LogStream();

    void stopListeningForIPC();

#if !ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
#endif

    LogStreamIdentifier identifier() const { return m_identifier; }

    static unsigned logCountForTesting();

private:
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    using ConnectionType = IPC::StreamServerConnection;
#else
    using ConnectionType = IPC::Connection;
#endif
    LogStream(WebProcessProxy&, Ref<ConnectionType>&&, LogStreamIdentifier);

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    // IPC::StreamServerConnection::Client overrides.
    void didReceiveInvalidMessage(IPC::StreamServerConnection&, IPC::MessageName, const Vector<uint32_t>&) final;
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;
#else
    // IPC::MessageReceiver overrides.
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
#endif

    void logOnBehalfOfWebContent(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, uint8_t logType);

#if __has_include("LogMessagesDeclarations.h")
#include "LogMessagesDeclarations.h"
#endif

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    const Ref<IPC::StreamServerConnection> m_connection;
    WeakPtr<WebProcessProxy> m_process;
#else
    ThreadSafeWeakPtr<IPC::Connection> m_connection;
#endif
    const LogStreamIdentifier m_identifier;
    const ProcessID m_pid;
};

} // namespace WebKit

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
