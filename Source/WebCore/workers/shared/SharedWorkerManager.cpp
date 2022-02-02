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
#include "SharedWorkerManager.h"

#include "ContentSecurityPolicy.h"
#include "EventNames.h"
#include "LoaderStrategy.h"
#include "MessageEvent.h"
#include "PlatformStrategies.h"
#include "SharedWorker.h"
#include "SharedWorkerGlobalScope.h"
#include "SharedWorkerScriptLoader.h"
#include "SharedWorkerThread.h"
#include "SharedWorkerThreadProxy.h"
#include "WorkerRunLoop.h"
#include "WorkerScriptLoader.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>

namespace WebCore {

SharedWorkerManager& SharedWorkerManager::singleton()
{
    static NeverDestroyed<SharedWorkerManager> instance;
    return instance.get();
}

void SharedWorkerManager::connect(const URL& url, SharedWorker& worker, TransferredMessagePort&& messagePort, WorkerOptions&& options)
{
    ASSERT(RunLoop::isMain());
    // FIXME: Handle workers from other processes, which will probably require moving connect to the FrameLoaderClient or something.
    // Maybe something more like what ServiceWorkers does.

    // FIXME: Check if we already made a thread.
    // FIXME: Check if there's already a shared worker and use that if there is.

    auto loader = makeUniqueRef<SharedWorkerScriptLoader>(url, worker, WTFMove(messagePort), WTFMove(options));
    auto identifier = loader->identifier();
    m_loaders.add(identifier, WTFMove(loader));
}

void SharedWorkerManager::scriptLoadFailed(SharedWorkerScriptLoader& loader)
{
    ActiveDOMObject::queueTaskToDispatchEvent(loader.worker(), TaskSource::DOMManipulation, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
    m_loaders.remove(loader.identifier());
}

void SharedWorkerManager::scriptLoadedSuccessfully(SharedWorkerScriptLoader& loader, const ScriptBuffer& scriptBuffer, ScriptExecutionContext& scriptExecutionContext, SharedWorker& sharedWorker, TransferredMessagePort&& transferredPort)
{
    ASSERT(RunLoop::isMain());
    auto& worker = loader.worker();
    auto& options = loader.options();

    auto& proxy = sharedWorker.proxy();

    proxy.startWorkerGlobalScope(
        loader.url(),
        options.name,
        scriptExecutionContext.userAgent(loader.url()),
        platformStrategies()->loaderStrategy()->isOnLine(),
        scriptBuffer,
        scriptExecutionContext.contentSecurityPolicy()->responseHeaders(),
        scriptExecutionContext.shouldBypassMainWorldContentSecurityPolicy(),
        scriptExecutionContext.crossOriginEmbedderPolicy(),
        worker.creationTime(),
        ReferrerPolicy::EmptyString,
        options.type,
        options.credentials,
        worker.runtimeFlags()
    );
    if (!proxy.thread())
        return;

    RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(is<Document>(scriptExecutionContext));
    auto sourceOrigin = downcast<Document>(scriptExecutionContext).securityOrigin().toString();

    proxy.thread()->runLoop().postTask([transferredPort = WTFMove(transferredPort), sourceOrigin = WTFMove(sourceOrigin).isolatedCopy()] (auto& scriptExecutionContext) mutable {
        ASSERT(!RunLoop::isMain());

        // https://html.spec.whatwg.org/multipage/workers.html#dom-sharedworker step 11.5
        auto serializedScriptValue = SerializedScriptValue::create("");
        ASSERT(serializedScriptValue);
        auto ports = MessagePort::entanglePorts(scriptExecutionContext, { WTFMove(transferredPort) });
        ASSERT(ports.size() == 1);
        auto port = ports[0];
        ASSERT(port);
        auto event = MessageEvent::create(WTFMove(ports), serializedScriptValue.releaseNonNull(), sourceOrigin, { }, port);
        event->initEvent(eventNames().connectEvent, false, false);

        RELEASE_ASSERT_WITH_SECURITY_IMPLICATION(is<SharedWorkerGlobalScope>(scriptExecutionContext));
        auto& workerGlobalScope = downcast<SharedWorkerGlobalScope>(scriptExecutionContext);
        workerGlobalScope.dispatchEvent(WTFMove(event));
    });

    m_loaders.remove(loader.identifier());
}

} // namespace WebCore
