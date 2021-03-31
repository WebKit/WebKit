/*
 * Copyright (C) 2008-2021 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
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
#include "WindowOrWorkerGlobalScopeIndexedDatabase.h"

#include "DOMWindow.h"
#include "DOMWindowProperty.h"
#include "Document.h"
#include "IDBConnectionProxy.h"
#include "IDBFactory.h"
#include "Page.h"
#include "Supplementable.h"
#include "WorkerGlobalScope.h"

namespace WebCore {

class DOMWindowIndexedDatabase : public DOMWindowProperty, public Supplement<DOMWindow> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DOMWindowIndexedDatabase(DOMWindow&);
    virtual ~DOMWindowIndexedDatabase() = default;

    static DOMWindowIndexedDatabase* from(DOMWindow&);
    IDBFactory* indexedDB();

private:
    static const char* supplementName() { return "DOMWindowIndexedDatabase"; }

    RefPtr<IDBFactory> m_idbFactory;
};

class WorkerGlobalScopeIndexedDatabase : public Supplement<WorkerGlobalScope> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WorkerGlobalScopeIndexedDatabase(IDBClient::IDBConnectionProxy&);
    virtual ~WorkerGlobalScopeIndexedDatabase() = default;

    static WorkerGlobalScopeIndexedDatabase* from(WorkerGlobalScope&);
    IDBFactory* indexedDB();

private:
    static const char* supplementName() { return "WorkerGlobalScopeIndexedDatabase"; }

    RefPtr<IDBFactory> m_idbFactory;
    Ref<IDBClient::IDBConnectionProxy> m_connectionProxy;
};

// DOMWindowIndexedDatabase supplement.

DOMWindowIndexedDatabase::DOMWindowIndexedDatabase(DOMWindow& window)
    : DOMWindowProperty(&window)
{
}

DOMWindowIndexedDatabase* DOMWindowIndexedDatabase::from(DOMWindow& window)
{
    auto* supplement = static_cast<DOMWindowIndexedDatabase*>(Supplement<DOMWindow>::from(&window, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<DOMWindowIndexedDatabase>(window);
        supplement = newSupplement.get();
        provideTo(&window, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

IDBFactory* DOMWindowIndexedDatabase::indexedDB()
{
    auto* window = this->window();
    if (!window)
        return nullptr;

    auto* document = window->document();
    if (!document)
        return nullptr;

    auto* page = document->page();
    if (!page)
        return nullptr;

    if (!window->isCurrentlyDisplayedInFrame())
        return nullptr;

    if (!m_idbFactory) {
        auto* connectionProxy = document->idbConnectionProxy();
        if (!connectionProxy)
            return nullptr;

        m_idbFactory = IDBFactory::create(*connectionProxy);
    }

    return m_idbFactory.get();
}

// WorkerGlobalScope supplement.

WorkerGlobalScopeIndexedDatabase::WorkerGlobalScopeIndexedDatabase(IDBClient::IDBConnectionProxy& connectionProxy)
    : m_connectionProxy(connectionProxy)
{
}

WorkerGlobalScopeIndexedDatabase* WorkerGlobalScopeIndexedDatabase::from(WorkerGlobalScope& scope)
{
    auto* supplement = static_cast<WorkerGlobalScopeIndexedDatabase*>(Supplement<WorkerGlobalScope>::from(&scope, supplementName()));
    if (!supplement) {
        auto* connectionProxy = scope.idbConnectionProxy();
        if (!connectionProxy)
            return nullptr;

        auto newSupplement = makeUnique<WorkerGlobalScopeIndexedDatabase>(*connectionProxy);
        supplement = newSupplement.get();
        provideTo(&scope, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

IDBFactory* WorkerGlobalScopeIndexedDatabase::indexedDB()
{
    if (!m_idbFactory)
        m_idbFactory = IDBFactory::create(m_connectionProxy.get());
    return m_idbFactory.get();
}


IDBFactory* WindowOrWorkerGlobalScopeIndexedDatabase::indexedDB(WorkerGlobalScope& scope)
{
    auto* scopeIDB = WorkerGlobalScopeIndexedDatabase::from(scope);
    return scopeIDB ? scopeIDB->indexedDB() : nullptr;
}

IDBFactory* WindowOrWorkerGlobalScopeIndexedDatabase::indexedDB(DOMWindow& window)
{
    return DOMWindowIndexedDatabase::from(window)->indexedDB();
}

} // namespace WebCore
