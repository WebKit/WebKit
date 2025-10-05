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

#include "config.h"
#include "LogClient.h"

#if ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)

#include "Connection.h"
#include "LogStreamMessages.h"
#include "StreamClientConnection.h"
#include "WebKitLogDefinitions.h"
#include <WebCore/WebCoreLogDefinitions.h>
#include <wtf/Locker.h>

namespace WebKit {

LogClient::LogClient(Ref<ConnectionType>&& connection)
    : m_connection(connection)
{
}

void LogClient::log(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, os_log_type_t type)
{
    send(Messages::LogStream::LogOnBehalfOfWebContent(logChannel, logCategory, logString, type));
}


#undef DEFINE_LOG_MESSAGE
#define DEFINE_LOG_MESSAGE(messageName, argumentDeclarations, arguments) \
void LogClient::messageName argumentDeclarations \
{ \
    send(Messages::LogStream::messageName arguments); \
}
WEBCORE_LOG_CLIENT_MESSAGES(DEFINE_LOG_MESSAGE);
WEBKIT2_LOG_CLIENT_MESSAGES(DEFINE_LOG_MESSAGE);
#undef DEFINE_LOG_MESSAGE

template<typename T>
void LogClient::send(T&& message)
{
#if ENABLE(STREAMING_IPC_IN_LOG_FORWARDING)
    Locker locker { m_lock };
#endif
    m_connection->send(WTFMove(message), identifier());
}

}

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
