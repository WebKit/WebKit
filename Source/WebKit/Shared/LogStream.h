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
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"

#include <wtf/RefPtr.h>

namespace IPC {
class StreamServerConnection;
struct StreamServerConnectionHandle;
}

namespace WebKit {

constexpr size_t logCategoryMaxSize = 32;
constexpr size_t logSubsystemMaxSize = 32;
constexpr size_t logStringMaxSize = 256;

class LogStream final
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
: public IPC::StreamMessageReceiver {
#else
: public RefCounted<LogStream>
, public IPC::MessageReceiver {
#endif
public:
    static Ref<LogStream> create(int32_t pid, LogStreamIdentifier identifier) { return adoptRef(*new LogStream(pid, identifier)); }
    ~LogStream();

    void stopListeningForIPC();

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    void setup(IPC::StreamServerConnectionHandle&&, CompletionHandler<void(IPC::Semaphore& streamWakeUpSemaphore, IPC::Semaphore& streamClientWaitSemaphore)>&&);
#else
    void setup(IPC::Connection&);
    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }
#endif

    LogStreamIdentifier identifier() const { return m_logStreamIdentifier; }

    static unsigned logCountForTesting();

private:
    LogStream(int32_t pid, LogStreamIdentifier);

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;
#else
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
#endif

    void logOnBehalfOfWebContent(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, uint8_t logType);

#if __has_include("LogMessagesDeclarations.h")
#include "LogMessagesDeclarations.h"
#endif

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    RefPtr<IPC::StreamServerConnection> m_logStreamConnection;
#else
    ThreadSafeWeakPtr<IPC::Connection> m_logConnection;
#endif
    LogStreamIdentifier m_logStreamIdentifier;
    int32_t m_pid { 0 };
};

} // namespace WebKit

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
