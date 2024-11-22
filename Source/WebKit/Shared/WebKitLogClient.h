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

#include "LogStream.h"
#include "LogStreamIdentifier.h"
#include "LogStreamMessages.h"
#include "StreamClientConnection.h"
#include <WebCore/WebCoreLogClient.h>
#include <wtf/Lock.h>

#include "WebCoreLogDefinitions.h"
#include "WebKitLogDefinitions.h"

namespace WebKit {

class WebKitLogClient : public WebCore::WebCoreLogClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebKitLogClient(IPC::StreamClientConnection*, const LogStreamIdentifier&);
    virtual ~WebKitLogClient() { };

#include "WebKitLogClientDeclarations.h"
#include "WebCoreLogClientDeclarations.h"

private:
    RefPtr<IPC::StreamClientConnection> m_logStreamConnection;
    LogStreamIdentifier m_logStreamIdentifier;
    Lock m_logStreamLock;
};

std::unique_ptr<WebKitLogClient>& webkitLogClient();

}
