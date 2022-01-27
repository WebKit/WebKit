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
#include "SharedWorker.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "MessageChannel.h"
#include "MessagePort.h"
#include "SecurityOrigin.h"
#include "SharedWorkerManager.h"
#include "SharedWorkerProxy.h"
#include "WorkerOptions.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SharedWorker);

ExceptionOr<Ref<SharedWorker>> SharedWorker::create(Document& document, JSC::RuntimeFlags runtimeFlags, String&& scriptURLString, std::optional<std::variant<String, WorkerOptions>>&& maybeOptions)
{
    if (!document.securityOrigin().canAccessSharedWorkers(document.topOrigin()))
        return Exception { SecurityError, "This iframe doesn't have storage access"_s };

    auto url = document.completeURL(scriptURLString);
    if (!url.isValid())
        return Exception { SyntaxError, "Invalid script URL"_s };

    if (auto* contentSecurityPolicy = document.contentSecurityPolicy()) {
        if (!contentSecurityPolicy->allowWorkerFromSource(url))
            return Exception { SecurityError };
    }

    WorkerOptions options;
    if (maybeOptions) {
        WTF::switchOn(*maybeOptions, [&] (const String& name) {
            options.name = name;
        }, [&] (const WorkerOptions& optionsFromVariant) {
            options = optionsFromVariant;
        });
    }

    auto channel = MessageChannel::create(document);
    auto transferredPort = channel->port2().disentangle();

    auto sharedWorker = adoptRef(*new SharedWorker(document, channel->port1(), runtimeFlags));
    sharedWorker->suspendIfNeeded();

    SharedWorkerManager::singleton().connect(url, sharedWorker.get(), WTFMove(transferredPort), WTFMove(options));
    return sharedWorker;
}

SharedWorker::SharedWorker(Document& document, Ref<MessagePort>&& port, JSC::RuntimeFlags runtimeFlags)
    : ActiveDOMObject(&document)
    , m_port(WTFMove(port))
    , m_identifierForInspector("SharedWorker:" + Inspector::IdentifiersFactory::createIdentifier())
    , m_runtimeFlags(runtimeFlags)
    , m_creationTime(MonotonicTime::now())
    , m_proxy(SharedWorkerProxy::create(*this))
{
}

SharedWorker::~SharedWorker()
{
    m_proxy.workerObjectDestroyed();
}

ScriptExecutionContext* SharedWorker::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

const char* SharedWorker::activeDOMObjectName() const
{
    return "SharedWorker";
}

EventTargetInterface SharedWorker::eventTargetInterface() const
{
    return SharedWorkerEventTargetInterfaceType;
}

void SharedWorker::stop()
{
    terminate();
}

bool SharedWorker::virtualHasPendingActivity() const
{
    return m_proxy.hasPendingActivity() || m_isLoading;
}

void SharedWorker::terminate()
{
    m_proxy.terminateWorkerGlobalScope();
}

} // namespace WebCore
