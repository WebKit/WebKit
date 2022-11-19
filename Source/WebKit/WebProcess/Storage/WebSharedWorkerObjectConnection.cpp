/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebSharedWorkerObjectConnection.h"

#include "Logging.h"
#include "NetworkProcessConnection.h"
#include "WebMessagePortChannelProvider.h"
#include "WebProcess.h"
#include "WebSharedWorkerServerConnectionMessages.h"
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/SharedWorkerKey.h>
#include <WebCore/WorkerOptions.h>

namespace WebKit {

#define CONNECTION_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - [webProcessIdentifier=%" PRIu64 "] WebSharedWorkerObjectConnection::" fmt, this, WebCore::Process::identifier().toUInt64(), ##__VA_ARGS__)

WebSharedWorkerObjectConnection::WebSharedWorkerObjectConnection()
{
    CONNECTION_RELEASE_LOG("WebSharedWorkerObjectConnection:");
}

WebSharedWorkerObjectConnection::~WebSharedWorkerObjectConnection()
{
    CONNECTION_RELEASE_LOG("~WebSharedWorkerObjectConnection:");
}

IPC::Connection* WebSharedWorkerObjectConnection::messageSenderConnection() const
{
    return &WebProcess::singleton().ensureNetworkProcessConnection().connection();
}

void WebSharedWorkerObjectConnection::requestSharedWorker(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier, WebCore::TransferredMessagePort&& port, const WebCore::WorkerOptions& workerOptions)
{
    CONNECTION_RELEASE_LOG("requestSharedWorker: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    WebMessagePortChannelProvider::singleton().messagePortSentToRemote(port.first);
    send(Messages::WebSharedWorkerServerConnection::RequestSharedWorker { sharedWorkerKey, sharedWorkerObjectIdentifier, WTFMove(port), workerOptions });
}

void WebSharedWorkerObjectConnection::sharedWorkerObjectIsGoingAway(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONNECTION_RELEASE_LOG("sharedWorkerObjectIsGoingAway: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    send(Messages::WebSharedWorkerServerConnection::SharedWorkerObjectIsGoingAway { sharedWorkerKey, sharedWorkerObjectIdentifier });
}

void WebSharedWorkerObjectConnection::suspendForBackForwardCache(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONNECTION_RELEASE_LOG("suspendForBackForwardCache: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    send(Messages::WebSharedWorkerServerConnection::SuspendForBackForwardCache { sharedWorkerKey, sharedWorkerObjectIdentifier });
}

void WebSharedWorkerObjectConnection::resumeForBackForwardCache(const WebCore::SharedWorkerKey& sharedWorkerKey, WebCore::SharedWorkerObjectIdentifier sharedWorkerObjectIdentifier)
{
    CONNECTION_RELEASE_LOG("resumeForBackForwardCache: sharedWorkerObjectIdentifier=%" PUBLIC_LOG_STRING, sharedWorkerObjectIdentifier.toString().utf8().data());
    send(Messages::WebSharedWorkerServerConnection::ResumeForBackForwardCache { sharedWorkerKey, sharedWorkerObjectIdentifier });
}

#undef CONNECTION_RELEASE_LOG

} // namespace WebKit
