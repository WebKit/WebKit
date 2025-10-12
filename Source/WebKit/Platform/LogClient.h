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

#include "Connection.h"
#include "LogStreamIdentifier.h"
#include "LogStreamMessages.h"
#include "StreamClientConnection.h"
#include "WebKitLogDefinitions.h"
#include <WebCore/LogClient.h>
#include <WebCore/WebCoreLogDefinitions.h>
#include <wtf/Identified.h>
#include <wtf/Lock.h>
#include <wtf/Locker.h>

namespace WebKit {

// Type which is captures WebKit and platform log calls and forwards log messages to another process.
// The messages are found from generated LogStream.messages.in in build directory,
// DerivedSources/WebKit/LogStream.messages.in.
class LogClient final : public WebCore::LogClient, public Identified<LogStreamIdentifier> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(LogClient);
public:
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    using ConnectionType = IPC::StreamClientConnection;
#else
    using ConnectionType = IPC::Connection;
#endif

    LogClient(Ref<ConnectionType>&&);

    void log(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, os_log_type_t) final;

#if __has_include("WebKitLogClientDeclarations.h")
#include "WebKitLogClientDeclarations.h"
#endif

#if __has_include("WebCoreLogClientDeclarations.h")
#include "WebCoreLogClientDeclarations.h"
#endif

private:
    bool isWebKitLogClient() const final { return true; }
    template<typename T>
    void send(T&& message);

#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    const Ref<IPC::StreamClientConnection> m_connection WTF_GUARDED_BY_LOCK(m_lock);
#if ENABLE(UNFAIR_LOCK)
    UnfairLock m_lock;
#else
    Lock m_lock;
#endif // ENABLE(UNFAIR_LOCK)
#else
    const Ref<IPC::Connection> m_connection;
#endif
};

template<typename T>
void LogClient::send(T&& message)
{
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    Locker locker { m_lock };
#endif
    m_connection->send(WTFMove(message), identifier());
}

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::LogClient)
static bool isType(const WebCore::LogClient& logClient) { return logClient.isWebKitLogClient(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
