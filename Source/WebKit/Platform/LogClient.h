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

#include "LogStream.h"
#include "LogStreamIdentifier.h"
#include "LogStreamMessages.h"
#include "StreamClientConnection.h"
#include <WebCore/LogClient.h>
#include <wtf/Lock.h>

#if __has_include("WebCoreLogDefinitions.h")
#include "WebCoreLogDefinitions.h"
#endif
#if __has_include("WebKitLogDefinitions.h")
#include "WebKitLogDefinitions.h"
#endif

namespace WebKit {

class LogClient final : public WebCore::LogClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    LogClient(IPC::StreamClientConnection&, const LogStreamIdentifier&);

    void log(std::span<const uint8_t> logChannel, std::span<const uint8_t> logCategory, std::span<const uint8_t> logString, os_log_type_t) final;

#if __has_include("WebKitLogClientDeclarations.h")
#include "WebKitLogClientDeclarations.h"
#endif

#if __has_include("WebCoreLogClientDeclarations.h")
#include "WebCoreLogClientDeclarations.h"
#endif

private:
    bool isWebKitLogClient() const final { return true; }

    const Ref<IPC::StreamClientConnection> m_logStreamConnection WTF_GUARDED_BY_LOCK(m_logStreamLock);
    LogStreamIdentifier m_logStreamIdentifier;
    Lock m_logStreamLock;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::LogClient)
static bool isType(const WebCore::LogClient& logClient) { return logClient.isWebKitLogClient(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(LOGD_BLOCKING_IN_WEBCONTENT)
