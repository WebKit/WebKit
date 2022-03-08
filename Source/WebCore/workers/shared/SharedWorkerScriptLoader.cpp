/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "SharedWorkerScriptLoader.h"

#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "SharedWorker.h"
#include "SharedWorkerManager.h"
#include "WorkerRunLoop.h"
#include "WorkerScriptLoader.h"

namespace WebCore {

SharedWorkerScriptLoader::SharedWorkerScriptLoader(const URL& url, SharedWorker& worker, TransferredMessagePort&& port, WorkerOptions&& options)
    : m_identifier(SharedWorkerScriptLoaderIdentifier::generate())
    , m_options(WTFMove(options))
    , m_worker(worker)
    , m_port(WTFMove(port))
    , m_loader(WorkerScriptLoader::create())
    , m_pendingActivity(worker.makePendingActivity(worker))
    , m_url(url)
{
    m_worker->setIsLoading(true);
    m_loader->loadAsynchronously(*worker.scriptExecutionContext(), ResourceRequest(url), worker.workerFetchOptions(options, FetchOptions::Destination::Sharedworker), ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective, ServiceWorkersMode::All, *this, WorkerRunLoop::defaultMode());
}

void SharedWorkerScriptLoader::didReceiveResponse(ResourceLoaderIdentifier identifier, const ResourceResponse&)
{
    InspectorInstrumentation::didReceiveScriptResponse(m_worker->scriptExecutionContext(), identifier);
}

void SharedWorkerScriptLoader::notifyFinished()
{
    m_worker->setIsLoading(false);

    auto* scriptExecutionContext = m_worker->scriptExecutionContext();
    if (m_loader->failed() || !scriptExecutionContext) {
        m_worker->dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
        SharedWorkerManager::singleton().scriptLoadFailed(*this); // deletes this.
    } else {
        InspectorInstrumentation::scriptImported(*scriptExecutionContext, m_loader->identifier(), m_loader->script().toString());
        SharedWorkerManager::singleton().scriptLoadedSuccessfully(*this, m_loader->script(), *m_worker->scriptExecutionContext(), m_worker.get(), std::exchange(m_port, { })); // deletes this.
    }
}

} // namespace WebCore
