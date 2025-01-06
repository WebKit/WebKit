/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "WorkerNavigator.h"

#include "Chrome.h"
#include "GPU.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "PushEvent.h"
#include "ServiceWorkerGlobalScope.h"
#include "WorkerBadgeProxy.h"
#include "WorkerGlobalScope.h"
#include "WorkerThread.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(WorkerNavigator);

WorkerNavigator::WorkerNavigator(ScriptExecutionContext& context, const String& userAgent, bool isOnline)
    : NavigatorBase(&context)
    , m_userAgent(userAgent)
    , m_isOnline(isOnline)
{
}

WorkerNavigator::~WorkerNavigator() = default;

const String& WorkerNavigator::userAgent() const
{
    return m_userAgent;
}

bool WorkerNavigator::onLine() const
{
    return m_isOnline;
}

GPU* WorkerNavigator::gpu()
{
#if HAVE(WEBGPU_IMPLEMENTATION)
    if (!m_gpuForWebGPU) {
        auto scriptExecutionContext = this->scriptExecutionContext();
        if (scriptExecutionContext->isWorkerGlobalScope()) {
            WorkerGlobalScope& workerGlobalScope = downcast<WorkerGlobalScope>(*scriptExecutionContext);
            if (!workerGlobalScope.graphicsClient())
                return nullptr;

            RefPtr gpu = workerGlobalScope.graphicsClient()->createGPUForWebGPU();
            if (!gpu)
                return nullptr;

            m_gpuForWebGPU = GPU::create(*gpu);
        } else if (scriptExecutionContext->isDocument()) {
            Ref document = downcast<Document>(*scriptExecutionContext);
            RefPtr page = document->page();
            if (!page)
                return nullptr;
            RefPtr gpu = page->chrome().createGPUForWebGPU();
            if (!gpu)
                return nullptr;

            m_gpuForWebGPU = GPU::create(*gpu);
        }
    }

    return m_gpuForWebGPU.get();
#else
    return nullptr;
#endif
}

void WorkerNavigator::setAppBadge(std::optional<unsigned long long> badge, Ref<DeferredPromise>&& promise)
{
#if ENABLE(DECLARATIVE_WEB_PUSH)
    if (is<ServiceWorkerGlobalScope>(scriptExecutionContext())) {
        if (RefPtr declarativePushEvent = downcast<ServiceWorkerGlobalScope>(scriptExecutionContext())->declarativePushEvent()) {
            declarativePushEvent->setUpdatedAppBadge(WTFMove(badge));
            return;
        }
    }
#endif // ENABLE(DECLARATIVE_WEB_PUSH)

    auto* scope = downcast<WorkerGlobalScope>(scriptExecutionContext());
    if (!scope) {
        promise->reject(ExceptionCode::InvalidStateError);
        return;
    }

    if (auto* workerBadgeProxy = scope->thread().workerBadgeProxy())
        workerBadgeProxy->setAppBadge(badge);
    promise->resolve();
}

void WorkerNavigator::clearAppBadge(Ref<DeferredPromise>&& promise)
{
    setAppBadge(0, WTFMove(promise));
}

} // namespace WebCore
