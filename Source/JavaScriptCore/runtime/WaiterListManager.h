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

#include "JSGlobalObject.h"
#include "JSPromise.h"
#include "ObjectConstructor.h"
#include "runtime/DeferredWorkTimer.h"
#include "runtime/JSCJSValue.h"
#include <memory>
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RawPointer.h>

namespace JSC {

namespace WaiterListsManagerInternal {
static constexpr bool verbose = false;
}

struct WaiterList;

struct Waiter {
    WTF_MAKE_FAST_ALLOCATED;

public:
    Waiter(bool isAsync)
        : isAsync(isAsync)
    {
    }

    Waiter* remove();

    bool isAsync { false };
    WaiterList* list { nullptr };
    Waiter* previous { nullptr };
    Waiter* next { nullptr };
};

struct SyncWaiter : Waiter {
    SyncWaiter(void* address)
        : Waiter(false)
        , address(address)
    {
    }

    ThreadCondition condition;
    Mutex lock;
    void* address;
};

struct AsyncWaiter : Waiter {
    AsyncWaiter(JSPromise* promise)
        : Waiter(true)
    {
        VM& vm = promise->globalObject()->vm();
        vm.apiLock().lock();
        ticket = vm.deferredWorkTimer->addPendingWork(vm, promise, {});
        vm.apiLock().unlock();
    }

    VM& getVM() const
    {
        return jsCast<JSPromise*>(ticket->target())->globalObject()->vm();
    }

    DeferredWorkTimer::Ticket ticket;
};

struct WaiterList {
    WTF_MAKE_FAST_ALLOCATED;

public:
    void addLast(Waiter* waiter)
    {
        RELEASE_ASSERT(!waiter->list && !waiter->previous && !waiter->next);
        if (head) {
            tail->next = waiter;
            waiter->previous = tail;
            tail = waiter;
        } else {
            RELEASE_ASSERT(!size);
            head = waiter;
            tail = waiter;
        }
        waiter->list = this;
        size++;
    }

    Waiter* takeFirst()
    {
        RELEASE_ASSERT(head && !head->previous && head->list == this);
        Waiter* result = head;
        if (head == tail) {
            head = nullptr;
            tail = nullptr;
        } else {
            head = head->next;
            head->previous = nullptr;
        }
        size--;
        return clear(result);
    }

    Waiter* takeLast()
    {
        RELEASE_ASSERT(tail && !tail->next && tail->list == this);
        Waiter* result = tail;
        if (head == tail) {
            head = nullptr;
            tail = nullptr;
        } else {
            tail = tail->previous;
            tail->next = nullptr;
        }
        size--;
        return clear(result);
    }

    Waiter* remove(Waiter* waiter)
    {
        RELEASE_ASSERT(waiter && waiter->list == this);
        if (waiter == head)
            return takeFirst();
        else if (waiter == tail)
            return takeLast();
        else {
            waiter->previous->next = waiter->next;
            waiter->next->previous = waiter->previous;
            size--;
            return clear(waiter);
        }
    }

    Waiter* clear(Waiter* waiter)
    {
        waiter->list = nullptr;
        waiter->previous = nullptr;
        waiter->next = nullptr;
        return waiter;
    }

    Lock lock;

    unsigned size { 0 };
    Waiter* head { nullptr };
    Waiter* tail { nullptr };
};

class WaiterListManager {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static WaiterListManager& singleton();

    void addWaiter(void* ptr, Waiter* waiter)
    {
        WaiterList* queue = add(ptr);
        RELEASE_ASSERT(queue);

        {
            LockHolder locker(queue->lock);
            queue->addLast(waiter);
        }

        if (WaiterListsManagerInternal::verbose)
            dataLogF("WaiterListManager added a new Waiter to a waiterList for ptr %p\n", ptr);
    }

    void addAsyncWaiter(void* ptr, JSPromise* promise, Seconds timeout)
    {
        WaiterList* queue = add(ptr);
        RELEASE_ASSERT(queue);

        AsyncWaiter* asyncWaiter = new AsyncWaiter(promise);
        {
            LockHolder locker(queue->lock);
            queue->addLast(asyncWaiter);
        }

        RunLoop::current().dispatchAfter(timeout, [asyncWaiter] {
            WaiterListManager::singleton().timeout(asyncWaiter);
        });

        if (WaiterListsManagerInternal::verbose)
            dataLogF("WaiterListManager added a new AsyncWaiter to a waiterList for ptr %p\n", ptr);
    }

    enum class ResolveResult : uint8_t { OK, TIMEOUT };
    unsigned notifyWaiter(void* ptr, unsigned count)
    {
        RELEASE_ASSERT(ptr);
        unsigned notified = 0;
        WaiterList* queue = find(ptr);
        if (queue) {
            LockHolder locker(queue->lock);
            while (notified < count && queue->size) {
                notifyWaiterImpl(locker, queue->takeFirst(), ResolveResult::OK);
                notified++;
            }
        }
        if (WaiterListsManagerInternal::verbose)
            dataLogF("WaiterListManager notified waiters (count %d) for ptr %p\n", notified, ptr);
        return notified;
    }

    void notifyWaiterImpl(const AbstractLocker&, Waiter* waiter, const ResolveResult resolveResult)
    {
        RELEASE_ASSERT(waiter);

        if (waiter->isAsync) {
            AsyncWaiter* asyncWaiter = static_cast<AsyncWaiter*>(waiter);
            VM& vm = asyncWaiter->getVM();
            auto ticket = asyncWaiter->ticket;
            vm.deferredWorkTimer->scheduleWorkSoon(ticket, [resolveResult](DeferredWorkTimer::Ticket ticket) {
                JSPromise* promise = jsCast<JSPromise*>(ticket->target());
                JSGlobalObject* globalObject = promise->globalObject();
                VM& vm = promise->globalObject()->vm();
                JSValue result = resolveResult == ResolveResult::OK ? vm.smallStrings.okString() : vm.smallStrings.timedOutString();
                promise->resolve(globalObject, result);
            });
            delete asyncWaiter;
            return;
        }

        SyncWaiter* syncWaiter = static_cast<SyncWaiter*>(waiter);
        {
            MutexLocker locker(syncWaiter->lock);
            syncWaiter->address = nullptr;
        }
        syncWaiter->condition.signal();
    }

    void timeout(AsyncWaiter* target)
    {
        LockHolder locker(target->list->lock);
        notifyWaiterImpl(locker, target->remove(), ResolveResult::TIMEOUT);
    }

    size_t waiterListSize(void* ptr)
    {
        WaiterList* queue = find(ptr);
        size_t size = 0;
        if (queue) {
            LockHolder locker(queue->lock);
            size = queue->size;
        }
        return size;
    }

private:
    WaiterList* add(void* ptr)
    {
        LockHolder locker(lock);
        return m_waiterLists.add(ptr, makeUnique<WaiterList>()).iterator->value.get();
    }

    WaiterList* find(void* ptr)
    {
        LockHolder locker(lock);
        WaiterList* result = nullptr;
        auto it = m_waiterLists.find(ptr);
        if (it != m_waiterLists.end())
            result = it->value.get();
        return result;
    }

    Lock lock;
    HashMap<void*, std::unique_ptr<WaiterList>> m_waiterLists;
};

} // namespace JSC
