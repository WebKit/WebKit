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

#include "config.h"
#include "WaiterListManager.h"

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

JSValue WaiterListManager::wait(JSGlobalObject* globalObject, VM& vm, void* ptr, bool didPassValidation, Seconds timeout, const AtomicsWait type)
{
    // bool didPassValidation = WTF::atomicLoad(ptr) == expectedValue;
    if (type == AtomicsWait::Async) {
        JSObject* object = constructEmptyObject(globalObject->vm(), globalObject->objectStructureForObjectConstructor());

        bool isAsync = false;
        JSValue value;
        if (!didPassValidation)
            value = vm.smallStrings.notEqualString();
        else if (!timeout)
            value = vm.smallStrings.timedOutString();
        else {
            isAsync = true;
            JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
            addAsyncWaiter(ptr, promise, timeout);
            value = promise;
        }

        object->putDirect(vm, vm.propertyNames->async, jsBoolean(isAsync));
        object->putDirect(vm, vm.propertyNames->value, value);
        return object;
    }

    if (didPassValidation) {
        Waiter waiter { &vm };
        WaiterList* list = addSyncWaiter(ptr, &waiter);
        const TimeWithDynamicClockType& time = MonotonicTime::now() + timeout;

        {
            LockHolder locker(list->lock);
            while (waiter.getVM() && time.nowWithSameClock() < time)
                waiter.getCondition().waitUntil(list->lock, time.approximateWallTime());
            RELEASE_ASSERT(!waiter.getVM() || waiter.getVM() == &vm);
            bool didGetDequeued = !waiter.getVM();
            if (didGetDequeued)
                return vm.smallStrings.okString();
            list->take(&waiter);
            return vm.smallStrings.timedOutString();
        }
    } else
        return vm.smallStrings.notEqualString();
}

WaiterList* WaiterListManager::addSyncWaiter(void* ptr, Waiter* waiter)
{
    WaiterList* list = findOrCreateList(ptr);
    waiter->getVM()->addElementPtr(ptr);

    {
        LockHolder locker(list->lock);
        list->addLast(waiter);
    }

    dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager added a new SyncWaiter ", RawPointer(waiter), " to a waiterList for ptr ", RawPointer(ptr));

    return list;
}

void WaiterListManager::addAsyncWaiter(void* ptr, JSPromise* promise, Seconds timeout)
{
    WaiterList* list = findOrCreateList(ptr);
    promise->vm().addElementPtr(ptr);

    Waiter* waiter = new Waiter(promise);
    {
        LockHolder locker(list->lock);
        list->addLast(waiter);
    }

    RunLoop::current().dispatchAfter(timeout, [ptr, waiter] {
        WaiterListManager::singleton().timeoutAsyncWaiter(ptr, waiter);
    });

    dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager added a new AsyncWaiter ", RawPointer(waiter), " to a waiterList for ptr ", RawPointer(ptr));
}

void WaiterListManager::timeoutAsyncWaiter(void* ptr, Waiter* target)
{
    RELEASE_ASSERT(ptr);
    WaiterList* list = findList(ptr);
    if (list) {
        LockHolder locker(list->lock);
        notifyWaiterImpl(locker, list->take(target), ResolveResult::TIMEOUT);
    }
}

unsigned WaiterListManager::notifyWaiter(void* ptr, unsigned count)
{
    RELEASE_ASSERT(ptr);
    unsigned notified = 0;
    WaiterList* list = findList(ptr);
    if (list) {
        LockHolder locker(list->lock);
        while (notified < count && list->size()) {
            notifyWaiterImpl(locker, list->takeFirst(), ResolveResult::OK);
            notified++;
        }
    }

    dataLogLnIf(WaiterListsManagerInternal::verbose, "WaiterListManager notified waiters (count ", notified, ") for ptr ", RawPointer(ptr));
    return notified;
}

void WaiterListManager::notifyWaiterImpl(const AbstractLocker&, Waiter* waiter, const ResolveResult resolveResult)
{
    RELEASE_ASSERT(waiter);

    if (waiter->isAsync()) {
        VM& vm = *waiter->getVM();
        auto ticket = waiter->getTicket();
        vm.deferredWorkTimer->scheduleWorkSoon(ticket, [resolveResult](DeferredWorkTimer::Ticket ticket) {
            JSPromise* promise = jsCast<JSPromise*>(ticket->target());
            JSGlobalObject* globalObject = promise->globalObject();
            VM& vm = promise->vm();
            JSValue result = resolveResult == ResolveResult::OK ? vm.smallStrings.okString() : vm.smallStrings.timedOutString();
            promise->resolve(globalObject, result);
        });
        delete waiter;
        return;
    }

    waiter->setVM(nullptr);
    waiter->getCondition().notifyOne();
}

size_t WaiterListManager::waiterListSize(void* ptr)
{
    WaiterList* list = findList(ptr);
    size_t size = 0;
    if (list) {
        LockHolder locker(list->lock);
        size = list->size();
    }
    return size;
}

void WaiterListManager::unregisterVM(VM* vm)
{
    UNUSED_PARAM(vm);
    LockHolder locker(m_waiterListsLock);
    for (void* ptr : vm->registeredElementPtrs()) {
        WaiterList* list = m_waiterLists.get(ptr);
        if (!list)
            continue;
        {
            LockHolder locker(list->lock);
            list->deleteIf([&](Waiter* waiter) {
                if (waiter->getVM() == vm) {
                    dataLogLnIf(WaiterListsManagerInternal::verbose,
                        "WaiterListManager::unregisterVM ",
                        (waiter->isAsync() ? " deleted AsyncWaiter " : " removed SyncWaiter "),
                        RawPointer(waiter),
                        " in WaiterList for ptr ",
                        RawPointer(ptr));
                    return true;
                }
                return false;
            });
        }
    }
}

void WaiterListManager::unregisterSharedArrayBuffer(uint8_t* arrayPtr, size_t size)
{
    UNUSED_PARAM(arrayPtr);
    LockHolder locker(m_waiterListsLock);
    m_waiterLists.removeIf([&](auto& entry) {
        return (entry.key >= arrayPtr && entry.key < arrayPtr + size);
    });
}

WaiterList* WaiterListManager::findOrCreateList(void* ptr)
{
    LockHolder locker(m_waiterListsLock);
    return m_waiterLists.add(ptr, makeUnique<WaiterList>()).iterator->value.get();
}

WaiterList* WaiterListManager::findList(void* ptr)
{
    LockHolder locker(m_waiterListsLock);
    WaiterList* result = nullptr;
    auto it = m_waiterLists.find(ptr);
    if (it != m_waiterLists.end())
        result = it->value.get();
    return result;
}

} // namespace JSC
