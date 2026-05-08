/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "WindowOrWorkerGlobalScopeScheduler.h"

#include "Document.h"
#include "ExceptionOr.h"
#include "LocalDOMWindow.h"
#include "LocalDOMWindowProperty.h"
#include "Scheduler.h"
#include "ScriptExecutionContext.h"
#include "Supplementable.h"
#include "WorkerGlobalScope.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class DOMWindowScheduler final : public Supplement<LocalDOMWindow>, public LocalDOMWindowProperty {
    WTF_MAKE_TZONE_ALLOCATED(DOMWindowScheduler);
public:
    explicit DOMWindowScheduler(LocalDOMWindow& window)
        : LocalDOMWindowProperty(&window)
    {
    }

    static DOMWindowScheduler* from(LocalDOMWindow& window)
    {
        auto* supplement = downcast<DOMWindowScheduler>(Supplement<LocalDOMWindow>::from(&window, supplementName()));
        if (!supplement) {
            auto newSupplement = makeUnique<DOMWindowScheduler>(window);
            supplement = newSupplement.get();
            provideTo(&window, supplementName(), WTF::move(newSupplement));
        }
        return supplement;
    }

    Scheduler& scheduler() const
    {
        if (!m_scheduler)
            m_scheduler = Scheduler::create();
        return *m_scheduler;
    }

private:
    static ASCIILiteral supplementName() { return "DOMWindowScheduler"_s; }
    bool isDOMWindowScheduler() const final { return true; }

    mutable RefPtr<Scheduler> m_scheduler;
};

class WorkerGlobalScopeScheduler final : public Supplement<WorkerGlobalScope> {
    WTF_MAKE_TZONE_ALLOCATED(WorkerGlobalScopeScheduler);
public:
    explicit WorkerGlobalScopeScheduler(WorkerGlobalScope&)
    {
    }

    static WorkerGlobalScopeScheduler* from(WorkerGlobalScope& scope)
    {
        auto* supplement = downcast<WorkerGlobalScopeScheduler>(Supplement<WorkerGlobalScope>::from(&scope, supplementName()));
        if (!supplement) {
            auto newSupplement = makeUnique<WorkerGlobalScopeScheduler>(scope);
            supplement = newSupplement.get();
            provideTo(&scope, supplementName(), WTF::move(newSupplement));
        }
        return supplement;
    }

    Scheduler& scheduler() const
    {
        if (!m_scheduler)
            m_scheduler = Scheduler::create();
        return *m_scheduler;
    }

private:
    static ASCIILiteral supplementName() { return "WorkerGlobalScopeScheduler"_s; }
    bool isWorkerGlobalScopeScheduler() const final { return true; }

    mutable RefPtr<Scheduler> m_scheduler;
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(DOMWindowScheduler);
WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerGlobalScopeScheduler);

ExceptionOr<Ref<Scheduler>> WindowOrWorkerGlobalScopeScheduler::scheduler(ScriptExecutionContext&, DOMWindow& window)
{
    RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localWindow)
        return Exception { ExceptionCode::InvalidStateError };

    if (!localWindow->isCurrentlyDisplayedInFrame())
        return Exception { ExceptionCode::InvalidStateError };

    return Ref { DOMWindowScheduler::from(*localWindow)->scheduler() };
}

Ref<Scheduler> WindowOrWorkerGlobalScopeScheduler::scheduler(ScriptExecutionContext&, WorkerGlobalScope& scope)
{
    return Ref { WorkerGlobalScopeScheduler::from(scope)->scheduler() };
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DOMWindowScheduler)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isDOMWindowScheduler(); }
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WorkerGlobalScopeScheduler)
    static bool isType(const WebCore::SupplementBase& supplement) { return supplement.isWorkerGlobalScopeScheduler(); }
SPECIALIZE_TYPE_TRAITS_END()
