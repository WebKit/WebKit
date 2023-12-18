/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 */

#include "config.h"
#include "Worklet.h"

#include "ContentSecurityPolicy.h"
#include "Document.h"
#include "JSDOMPromiseDeferred.h"
#include "Page.h"
#include "ScriptSourceCode.h"
#include "SecurityOrigin.h"
#include "WorkerRunLoop.h"
#include "WorkletGlobalScope.h"
#include "WorkletGlobalScopeProxy.h"
#include "WorkletPendingTasks.h"
#include <JavaScriptCore/IdentifiersFactory.h>
#include <wtf/CrossThreadCopier.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Worklet);

Worklet::Worklet(Document& document)
    : ActiveDOMObject(&document)
    , m_identifier("worklet:" + Inspector::IdentifiersFactory::createIdentifier())
{
}

Worklet::~Worklet() = default;

Document* Worklet::document()
{
    return downcast<Document>(scriptExecutionContext());
}

// https://www.w3.org/TR/worklets-1/#dom-worklet-addmodule
void Worklet::addModule(const String& moduleURLString, WorkletOptions&& options, DOMPromiseDeferred<void>&& promise)
{
    auto* document = this->document();
    if (!document || !document->page()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "This frame is detached"_s });
        return;
    }

    URL moduleURL = document->completeURL(moduleURLString);
    if (!moduleURL.isValid()) {
        promise.reject(Exception { ExceptionCode::SyntaxError, "Module URL is invalid"_s });
        return;
    }

    if (!document->checkedContentSecurityPolicy()->allowScriptFromSource(moduleURL)) {
        promise.reject(Exception { ExceptionCode::SecurityError, "Not allowed by CSP"_s });
        return;
    }

    if (m_proxies.isEmpty())
        m_proxies.appendVector(createGlobalScopes());

    auto pendingTasks = WorkletPendingTasks::create(*this, WTFMove(promise), m_proxies.size());
    m_pendingTasksSet.add(pendingTasks.copyRef());

    for (auto& proxy : m_proxies) {
        proxy->postTaskForModeToWorkletGlobalScope([pendingTasks = pendingTasks.copyRef(), moduleURL = moduleURL.isolatedCopy(), credentials = options.credentials, pendingActivity = makePendingActivity(*this)](ScriptExecutionContext& context) mutable {
            downcast<WorkletGlobalScope>(context).fetchAndInvokeScript(moduleURL, credentials, [pendingTasks = WTFMove(pendingTasks), pendingActivity = WTFMove(pendingActivity)](std::optional<Exception>&& exception) mutable {
                callOnMainThread([pendingTasks = WTFMove(pendingTasks), exception = crossThreadCopy(WTFMove(exception)), pendingActivity = WTFMove(pendingActivity)]() mutable {
                    if (exception)
                        pendingTasks->abort(WTFMove(*exception));
                    else
                        pendingTasks->decrementCounter();
                });
            });
        }, WorkerRunLoop::defaultMode());
    }
}

void Worklet::finishPendingTasks(WorkletPendingTasks& tasks)
{
    ASSERT(isMainThread());
    ASSERT(m_pendingTasksSet.contains(&tasks));

    m_pendingTasksSet.remove(&tasks);
}

const char* Worklet::activeDOMObjectName() const
{
    return "Worklet";
}

} // namespace WebCore
