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

#include "config.h"
#include "WaiterListManager.h"

#include "JSCInlines.h"
#include "JSGlobalObject.h"
#include "JSLock.h"
#include "ObjectConstructor.h"
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RawPointer.h>

namespace JSC {

namespace WaiterListsManagerInternal {
static constexpr bool verbose = false;
}

WaiterListManager& WaiterListManager::singleton()
{
    static LazyNeverDestroyed<WaiterListManager> manager;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        manager.construct();
    });
    return manager;
}

template <typename ValueType>
JSValue WaiterListManager::waitImpl(JSGlobalObject* globalObject, VM& vm, ValueType* ptr, ValueType expectedValue, Seconds timeout, AtomicsWaitType type)
{
    if (type == AtomicsWaitType::Async) {
        JSObject* object = constructEmptyObject(globalObject);

        bool isAsync = false;
        JSValue value;

        Ref<WaiterList> list = findOrCreateList(ptr);
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

        {
            Locker listLocker { list->lock };
            if (WTF::atomicLoad(ptr) != expectedValue)
                value = vm.smallStrings.notEqualString();
            else if (!timeout)
                value = vm.smallStrings.timedOutString();
            else {
                isAsync = true;

                Ref<Waiter> waiter = adoptRef(*new Waiter(promise));
                list->addLast(listLocker, waiter);

                if (timeout != Seconds::infinity()) {
                    Ref<RunLoop::DispatchTimer> timer = RunLoop::current().dispatchAfter(timeout, [this, ptr, waiter = waiter.copyRef()]() mutable {
                        timeoutAsyncWaiter(ptr, WTFMove(waiter));
                    });
                    waiter->setTimer(listLocker, WTFMove(timer));
                    dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager added a new AsyncWaiter ", RawPointer(waiter.ptr()), " to a waiterList for ptr ", RawPointer(ptr));
                }

                value = promise;
            }
        }

        object->putDirect(vm, vm.propertyNames->async, jsBoolean(isAsync));
        object->putDirect(vm, vm.propertyNames->value, value);
        return object;
    }

    Ref<Waiter> syncWaiter = vm.syncWaiter();
    Ref<WaiterList> list = findOrCreateList(ptr);
    MonotonicTime time = MonotonicTime::now() + timeout;

    {
        Locker listLocker { list->lock };
        if (WTF::atomicLoad(ptr) != expectedValue)
            return vm.smallStrings.notEqualString();

        list->addLast(listLocker, syncWaiter);
        dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager added a new SyncWaiter ", RawPointer(&syncWaiter), " to a waiterList for ptr ", RawPointer(ptr));

        while (syncWaiter->vm() && time.now() < time)
            syncWaiter->condition().waitUntil(list->lock, time.approximateWallTime());

        // At this point, syncWaiter should be either notified (dequeued) or timeout (not dequeued).
        // If it's notified by other thread, it's vm should be nulled out.
        bool didGetDequeued = !syncWaiter->vm();
        ASSERT(didGetDequeued || syncWaiter->vm() == &vm);
        if (didGetDequeued)
            return vm.smallStrings.okString();

        didGetDequeued = list->findAndRemove(listLocker, syncWaiter);
        ASSERT(didGetDequeued);
        return vm.smallStrings.timedOutString();
    }
}

JSValue WaiterListManager::wait(JSGlobalObject* globalObject, VM& vm, int32_t* ptr, int32_t expected, Seconds timeout, AtomicsWaitType waitType)
{
    return waitImpl(globalObject, vm, ptr, expected, timeout, waitType);
}

JSValue WaiterListManager::wait(JSGlobalObject* globalObject, VM& vm, int64_t* ptr, int64_t expected, Seconds timeout, AtomicsWaitType waitType)
{
    return waitImpl(globalObject, vm, ptr, expected, timeout, waitType);
}

void WaiterListManager::timeoutAsyncWaiter(void* ptr, Ref<Waiter>&& waiter)
{
    if (RefPtr<WaiterList> list = findList(ptr)) {
        // All cases:
        // 1. Find a list for ptr.
        Locker listLocker { list->lock };
        if (waiter->ticket(listLocker)) {
            if (waiter->isOnList()) {
                // 1.1. The list contains the waiter which must be in the list and hasn't been notified.
                //      It should have a ticket, then notify it with timeout.
                bool didGetDequeued = list->findAndRemove(listLocker, waiter);
                ASSERT_UNUSED(didGetDequeued, didGetDequeued);
            }
            // 1.2. The list doesn't contain the waiter.
            //      1.2.1 It's a new list, then the waiter must be removed from a list which is destructed.
            //            Then, the waiter may (notify it if it does) or may not have a ticket.
            notifyWaiterImpl(listLocker,  WTFMove(waiter), ResolveResult::Timeout);
            return;
        }
        // 1.2.2 It's the list the waiter used to belong. Then it must be notified by other thread and ignore it.
    }

    // 2. Doesn't find a list for ptr, then the waiter must be removed from the list.
    //      2.1. The waiter has a ticket, then notify it.
    //      2.2. The waiter doesn't has a ticket, then it's notified and ignore it.
    ASSERT(!waiter->isOnList());
    if (waiter->ticket(NoLockingNecessary))
        notifyWaiterImpl(NoLockingNecessary, WTFMove(waiter), ResolveResult::Timeout);
}

unsigned WaiterListManager::notifyWaiter(void* ptr, unsigned count)
{
    ASSERT(ptr);
    unsigned notified = 0;
    RefPtr<WaiterList> list = findList(ptr);
    if (list) {
        Locker listLocker { list->lock };
        while (notified < count && list->size()) {
            notifyWaiterImpl(listLocker, list->takeFirst(listLocker), ResolveResult::Ok);
            notified++;
        }
    }

    dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager notified waiters (count ", notified, ") for ptr ", RawPointer(ptr));
    return notified;
}

void WaiterListManager::notifyWaiterImpl(const AbstractLocker& listLocker, Ref<Waiter>&& waiter, const ResolveResult resolveResult)
{
    if (waiter->isAsync()) {
        VM& vm = *waiter->vm();
        auto ticket = waiter->takeTicket(listLocker);
        ASSERT(ticket);
        vm.deferredWorkTimer->scheduleWorkSoon(ticket, [resolveResult](DeferredWorkTimer::Ticket ticket) {
            JSPromise* promise = jsCast<JSPromise*>(ticket->target());
            JSGlobalObject* globalObject = promise->globalObject();
            VM& vm = promise->vm();
            JSValue result = resolveResult == ResolveResult::Ok ? vm.smallStrings.okString() : vm.smallStrings.timedOutString();
            promise->resolve(globalObject, result);
        });

        // If waiter is an AsyncWaiter, we null out its ticket first to indicate that it's notified.
        // Then, cancel its RunLoop timer if it's not timed-out.
        if (resolveResult != ResolveResult::Timeout)
            waiter->cancelTimer(listLocker);

        return;
    }

    // If waiter is a SyncWaiter, we null out its vm to indicate that this waiter
    // is removed from the WaiterList.
    ASSERT(waiter->vm());
    waiter->clearVM(listLocker);
    waiter->condition().notifyOne();
}

size_t WaiterListManager::waiterListSize(void* ptr)
{
    RefPtr<WaiterList> list = findList(ptr);
    size_t size = 0;
    if (list) {
        Locker listLocker { list->lock };
        size = list->size();
    }
    return size;
}

void WaiterListManager::cancelAsyncWaiter(const AbstractLocker& listLocker, Waiter* waiter)
{
    ASSERT(waiter->isAsync());
    waiter->vm()->deferredWorkTimer->scheduleWorkSoon(waiter->takeTicket(listLocker), [](DeferredWorkTimer::Ticket) mutable { });
    waiter->cancelTimer(listLocker);
}

void WaiterListManager::unregisterVM(VM* vm)
{
    Locker waiterListsLocker { m_waiterListsLock };
    for (auto& entry : m_waiterLists) {
        Ref<WaiterList> list = entry.value;
        Locker listLocker { list->lock };
        list->removeIf(listLocker, [&](Waiter* waiter) {
            if (waiter->vm() == vm) {
                // If the vm is about destructing, then it shouldn't
                // been blocked. That means we shouldn't find any SyncWaiter.
                ASSERT(waiter->isAsync());
                cancelAsyncWaiter(listLocker, waiter);

                dataLogLnIf(WaiterListsManagerInternal::verbose,
                    "WaiterListManager::unregisterVM ",
                    (waiter->isAsync() ? " deleted AsyncWaiter " : " removed SyncWaiter "),
                    RawPointer(waiter),
                    " in WaiterList for ptr ",
                    RawPointer(entry.key));

                return true;
            }
            return false;
        });
    }
}

void WaiterListManager::unregisterSharedArrayBuffer(uint8_t* arrayPtr, size_t size)
{
    Locker listLocker { m_waiterListsLock };
    m_waiterLists.removeIf([&](auto& entry) {
        if (entry.key >= arrayPtr && entry.key < arrayPtr + size) {
            Ref<WaiterList> list = entry.value;
            Locker listLocker { list->lock };
            list->removeIf(listLocker, [&](Waiter* waiter) {
                // If the SharedArrayBuffer is about destructing, then no VM is
                // referencing the buffer. That means no blocking SyncWaiter
                // on the buffer for any VM.
                ASSERT(waiter->isAsync());
                // If the AsyncWaiter has valid timer, then let it
                // timeout. Otherwise un-task it.
                if (!waiter->hasTimer(listLocker))
                    cancelAsyncWaiter(listLocker, waiter);

                dataLogLnIf(WaiterListsManagerInternal::verbose,
                    "WaiterListManager::unregisterSharedArrayBuffer ",
                    (waiter->isAsync() ? " deleted AsyncWaiter " : " removed SyncWaiter "),
                    RawPointer(waiter),
                    " in WaiterList for ptr ",
                    RawPointer(entry.key));

                return true;
            });

            ASSERT(!list->size());
            return true;
        }
        return false;
    });
}

Ref<WaiterList> WaiterListManager::findOrCreateList(void* ptr)
{
    Locker waiterListsLocker { m_waiterListsLock };
    return m_waiterLists.ensure(ptr, [] {
        return adoptRef(*new WaiterList()); 
    }).iterator->value.get();
}

RefPtr<WaiterList> WaiterListManager::findList(void* ptr)
{
    Locker waiterListsLocker { m_waiterListsLock };
    auto it = m_waiterLists.find(ptr);
    if (it == m_waiterLists.end())
        return nullptr;
    return it->value.ptr();
}

} // namespace JSC
