/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#pragma once

#include "DeferredWorkTimer.h"
#include "JSPromise.h"
#include <wtf/Condition.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

enum class AtomicsWait : uint8_t { Sync,
    Async };

class Waiter final : public WTF::BasicRawSentinelNode<Waiter> {
    WTF_MAKE_FAST_ALLOCATED;

public:
    Waiter(VM* vm)
        : m_vm(vm)
        , m_isAsync(false)
    {
    }

    Waiter(JSPromise* promise)
        : m_isAsync(true)
    {
        VM& vm = promise->vm();
        vm.apiLock().lock();
        m_ticket = vm.deferredWorkTimer->addPendingWork(vm, promise, { });
        vm.apiLock().unlock();
    }

    VM* getVM()
    {
        return m_isAsync ? &jsCast<JSPromise*>(m_ticket->target())->vm() : m_vm;
    }

    void setVM(VM* vm)
    {
        m_vm = vm;
    }

    DeferredWorkTimer::Ticket getTicket()
    {
        return m_ticket;
    }

    Condition& getCondition()
    {
        return m_condition;
    }

    bool isAsync()
    {
        return m_isAsync;
    }

private:
    VM* m_vm { nullptr };
    DeferredWorkTimer::Ticket m_ticket;
    Condition m_condition;
    bool m_isAsync { false };
};

class WaiterList {
    WTF_MAKE_FAST_ALLOCATED;

public:
    ~WaiterList()
    {
        deleteIf([] (auto&) {
            return true;
        });
    }

    void addLast(Waiter* waiter)
    {
        m_waiters.append(waiter);
        m_size++;
    }

    Waiter* takeFirst()
    {
        Waiter* result = &*m_waiters.begin();
        return take(result);
    }

    Waiter* takeLast()
    {
        Waiter* result = &*(--m_waiters.end());
        return take(result);
    }

    Waiter* take(Waiter* waiter)
    {
        m_waiters.remove(waiter);
        m_size--;
        return waiter;
    }

    template<typename Functor>
    void deleteIf(const Functor& functor)
    {
        m_waiters.forEach([&](Waiter* waiter) {
            if (functor(waiter)) {
                take(waiter);
                if (waiter->isAsync())
                    delete waiter;
            }
        });
    }

    unsigned size()
    {
        return m_size;
    }

    Lock lock;

private:
    unsigned m_size { 0 };
    SentinelLinkedList<Waiter, BasicRawSentinelNode<Waiter>> m_waiters;
};

class WaiterListManager {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static WaiterListManager& singleton();

    JS_EXPORT_PRIVATE JSValue wait(JSGlobalObject*, VM&, void* ptr, bool didPassValidation, Seconds timeout, const AtomicsWait type);

    WaiterList* addSyncWaiter(void* ptr, Waiter*);

    void addAsyncWaiter(void* ptr, JSPromise*, Seconds timeout);

    enum class ResolveResult : uint8_t { OK, TIMEOUT };
    unsigned notifyWaiter(void* ptr, unsigned count);

    size_t waiterListSize(void* ptr);

    void unregisterVM(VM*);

    void unregisterSharedArrayBuffer(uint8_t* arrayPtr, size_t);

private:
    void notifyWaiterImpl(const AbstractLocker&, Waiter*, const ResolveResult);

    void timeoutAsyncWaiter(void* ptr, Waiter* target);

    WaiterList* findOrCreateList(void* ptr);

    WaiterList* findList(void* ptr);

    Lock m_waiterListsLock;
    HashMap<void*, std::unique_ptr<WaiterList>> m_waiterLists;
};

} // namespace JSC
