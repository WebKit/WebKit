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
#include "SharedWorkerScriptLoader.h"

#include "EventNames.h"
#include "InspectorInstrumentation.h"
#include "SharedWorker.h"
#include "WorkerFetchResult.h"
#include "WorkerInitializationData.h"
#include "WorkerRunLoop.h"
#include "WorkerScriptLoader.h"

namespace WebCore {

SharedWorkerScriptLoader::SharedWorkerScriptLoader(URL&& url, SharedWorker& worker, WorkerOptions&& options)
    : m_options(WTFMove(options))
    , m_worker(worker)
    , m_loader(WorkerScriptLoader::create())
    , m_url(WTFMove(url))
{
}

void SharedWorkerScriptLoader::load(CompletionHandler<void(WorkerFetchResult&&, WorkerInitializationData&&)>&& completionHandler)
{
    ASSERT(!m_completionHandler);
    m_completionHandler = WTFMove(completionHandler);

    auto source = m_options.type == WorkerType::Module ? WorkerScriptLoader::Source::ModuleScript : WorkerScriptLoader::Source::ClassicWorkerScript;
    m_loader->loadAsynchronously(*m_worker->scriptExecutionContext(), ResourceRequest(m_url), source, m_worker->workerFetchOptions(m_options, FetchOptions::Destination::Sharedworker), ContentSecurityPolicyEnforcement::EnforceWorkerSrcDirective, ServiceWorkersMode::All, *this, WorkerRunLoop::defaultMode(), ScriptExecutionContextIdentifier::generate());
}

void SharedWorkerScriptLoader::didReceiveResponse(ResourceLoaderIdentifier identifier, const ResourceResponse&)
{
    InspectorInstrumentation::didReceiveScriptResponse(m_worker->scriptExecutionContext(), identifier);
}

void SharedWorkerScriptLoader::notifyFinished()
{
    auto* scriptExecutionContext = m_worker->scriptExecutionContext();
    if (scriptExecutionContext && !m_loader->failed())
        InspectorInstrumentation::scriptImported(*scriptExecutionContext, m_loader->identifier(), m_loader->script().toString());

    auto fetchResult = m_loader->fetchResult();
    if (fetchResult.referrerPolicy.isNull() && scriptExecutionContext)
        fetchResult.referrerPolicy = referrerPolicyToString(scriptExecutionContext->referrerPolicy());
    m_completionHandler(WTFMove(fetchResult), WorkerInitializationData {
        m_loader->takeServiceWorkerData(),
        m_loader->clientIdentifier(),
        m_loader->userAgentForSharedWorker()
    }); // deletes this.
}

} // namespace WebCore
