/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "SharedWorkerGlobalScope.h"

#include "EventNames.h"
#include "Logging.h"
#include "MessageEvent.h"
#include "SerializedScriptValue.h"
#include "ServiceWorkerThread.h"
#include "SharedWorkerThread.h"
#include "WorkerThread.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SharedWorkerGlobalScope);

#define SCOPE_RELEASE_LOG(fmt, ...) RELEASE_LOG(SharedWorker, "%p - [sharedWorkerIdentifier=%" PRIu64 "] SharedWorkerGlobalScope::" fmt, this, this->thread().identifier().toUInt64(), ##__VA_ARGS__)

SharedWorkerGlobalScope::SharedWorkerGlobalScope(const String& name, const WorkerParameters& params, Ref<SecurityOrigin>&& origin, SharedWorkerThread& thread, Ref<SecurityOrigin>&& topOrigin, IDBClient::IDBConnectionProxy* connectionProxy, SocketProvider* socketProvider, std::unique_ptr<WorkerClient>&& workerClient)
    : WorkerGlobalScope(WorkerThreadType::SharedWorker, params, WTFMove(origin), thread, WTFMove(topOrigin), connectionProxy, socketProvider, WTFMove(workerClient))
    , m_name(name)
{
    SCOPE_RELEASE_LOG("SharedWorkerGlobalScope:");
    relaxAdoptionRequirement();
    applyContentSecurityPolicyResponseHeaders(params.contentSecurityPolicyResponseHeaders);
}

SharedWorkerGlobalScope::~SharedWorkerGlobalScope()
{
    // We need to remove from the contexts map very early in the destructor so that calling postTask() on this WorkerGlobalScope from another thread is safe.
    removeFromContextsMap();
}

SharedWorkerThread& SharedWorkerGlobalScope::thread()
{
    return static_cast<SharedWorkerThread&>(WorkerGlobalScope::thread());
}

// https://html.spec.whatwg.org/multipage/workers.html#dom-sharedworker step 11.5
void SharedWorkerGlobalScope::postConnectEvent(TransferredMessagePort&& transferredPort, const String& sourceOrigin)
{
    SCOPE_RELEASE_LOG("postConnectEvent:");
    auto ports = MessagePort::entanglePorts(*this, { WTFMove(transferredPort) });
    ASSERT(ports.size() == 1);
    auto port = ports[0];
    ASSERT(port);
    auto event = MessageEvent::create(emptyString(), sourceOrigin, { }, port, WTFMove(ports));
    event->initEvent(eventNames().connectEvent, false, false);

    dispatchEvent(WTFMove(event));
}

#undef SCOPE_RELEASE_LOG

} // namespace WebCore
