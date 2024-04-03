/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "WindowOrWorkerGlobalScopeCaches.h"

#include "CacheStorageProvider.h"
#include "DOMCacheStorage.h"
#include "Document.h"
#include "LocalDOMWindow.h"
#include "LocalDOMWindowProperty.h"
#include "LocalFrame.h"
#include "Page.h"
#include "Supplementable.h"
#include "WorkerCacheStorageConnection.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

class DOMWindowCaches : public Supplement<LocalDOMWindow>, public LocalDOMWindowProperty {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DOMWindowCaches(LocalDOMWindow&);
    virtual ~DOMWindowCaches() = default;

    static DOMWindowCaches* from(LocalDOMWindow&);
    DOMCacheStorage* caches() const;

private:
    static ASCIILiteral supplementName() { return "DOMWindowCaches"_s; }

    mutable RefPtr<DOMCacheStorage> m_caches;
};

class WorkerGlobalScopeCaches : public Supplement<WorkerGlobalScope> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerGlobalScopeCaches(WorkerGlobalScope&);
    virtual ~WorkerGlobalScopeCaches() = default;

    static WorkerGlobalScopeCaches* from(WorkerGlobalScope&);
    DOMCacheStorage* caches() const;

private:
    static ASCIILiteral supplementName() { return "WorkerGlobalScopeCaches"_s; }

    WorkerGlobalScope& m_scope;
    mutable RefPtr<DOMCacheStorage> m_caches;
};

// DOMWindowCaches supplement.

DOMWindowCaches::DOMWindowCaches(LocalDOMWindow& window)
    : LocalDOMWindowProperty(&window)
{
}

DOMWindowCaches* DOMWindowCaches::from(LocalDOMWindow& window)
{
    auto* supplement = static_cast<DOMWindowCaches*>(Supplement<LocalDOMWindow>::from(&window, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMWindowCaches>(window);
        supplement = newSupplement.get();
        provideTo(&window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

DOMCacheStorage* DOMWindowCaches::caches() const
{
    ASSERT(frame());
    ASSERT(frame()->document());
    if (!m_caches && frame()->page())
        m_caches = DOMCacheStorage::create(*frame()->document(), frame()->page()->cacheStorageProvider().createCacheStorageConnection());
    return m_caches.get();
}

// WorkerGlobalScope supplement.

WorkerGlobalScopeCaches::WorkerGlobalScopeCaches(WorkerGlobalScope& scope)
    : m_scope(scope)
{
}

WorkerGlobalScopeCaches* WorkerGlobalScopeCaches::from(WorkerGlobalScope& scope)
{
    auto* supplement = static_cast<WorkerGlobalScopeCaches*>(Supplement<WorkerGlobalScope>::from(&scope, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<WorkerGlobalScopeCaches>(scope);
        supplement = newSupplement.get();
        provideTo(&scope, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

DOMCacheStorage* WorkerGlobalScopeCaches::caches() const
{
    if (!m_caches)
        m_caches = DOMCacheStorage::create(m_scope, m_scope.cacheStorageConnection());
    return m_caches.get();
}

// WindowOrWorkerGlobalScopeCaches.

ExceptionOr<DOMCacheStorage*> WindowOrWorkerGlobalScopeCaches::caches(ScriptExecutionContext& context, LocalDOMWindow& window)
{
    if (downcast<Document>(context).isSandboxed(SandboxOrigin))
        return Exception { ExceptionCode::SecurityError, "Cache storage is disabled because the context is sandboxed and lacks the 'allow-same-origin' flag"_s };

    if (!window.isCurrentlyDisplayedInFrame())
        return nullptr;

    return DOMWindowCaches::from(window)->caches();
}

DOMCacheStorage* WindowOrWorkerGlobalScopeCaches::caches(ScriptExecutionContext&, WorkerGlobalScope& scope)
{
    return WorkerGlobalScopeCaches::from(scope)->caches();
}

} // namespace WebCore
