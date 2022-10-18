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
#include "runtime/DeferredWorkTimer.h"
#include <wtf/DataLog.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/ParkingLot.h>
#include <wtf/RawPointer.h>

namespace JSC {

namespace WaiterListsManagerInternal {
static constexpr bool verbose = false;
}

class WaiterListManager {
    WTF_MAKE_FAST_ALLOCATED;

public:
    static WaiterListManager& singleton();

    void addWaiter(void* ptr, JSPromise* promise, Seconds timeout)
    {
        RELEASE_ASSERT(ptr && promise);
        VM& vm = promise->globalObject()->vm();
        vm.apiLock().lock();
        auto ticket = vm.deferredWorkTimer->addPendingWork(vm, promise, { });
        vm.apiLock().unlock();

        ParkingLot::parkAsync(ptr, ticket);

        m_registeredVMs.add(&vm);
        RunLoop::current().dispatchAfter(timeout, [ptr, ticket] {
            WaiterListManager::singleton().timeout(ptr, ticket);
        });

        if (WaiterListsManagerInternal::verbose)
            dataLogF("WaiterListManager added a new AsyncWaiter to a waiterList for ptr %p\n", ptr);
    }

    void timeout(void* ptr, DeferredWorkTimer::Ticket ticket)
    {
        RELEASE_ASSERT(ptr && ticket);
        JSPromise* promise = jsCast<JSPromise*>(ticket->target());
        VM* vm = &promise->globalObject()->vm();
        if (!m_registeredVMs.contains(vm))
            return;

        ParkingLot::unparkAsync(ptr, ticket, [this](void* data) {
            bool isOKString = false;
            notifyWaiterImpl(data, isOKString);
        });
    }

    unsigned notifyWaiter(void* ptr, unsigned count)
    {
        RELEASE_ASSERT(ptr);
        bool isAsyncMode = true;
        unsigned notified = ParkingLot::unparkCount(ptr, count, [this](void* data) {
            bool isOKString = true;
            notifyWaiterImpl(data, isOKString);
        }, isAsyncMode);
        if (WaiterListsManagerInternal::verbose)
            dataLogF("WaiterListManager notified waiters (count %d) for ptr %p\n", notified, ptr);
        return notified;
    }

    void notifyWaiterImpl(void* data, const bool isOKString)
    {
        auto ticket = static_cast<DeferredWorkTimer::Ticket>(data);
        VM* vm = &jsCast<JSPromise*>(ticket->target())->globalObject()->vm();
        vm->deferredWorkTimer->scheduleWorkSoon(ticket, [isOKString](DeferredWorkTimer::Ticket ticket) {
            JSPromise* promise = jsCast<JSPromise*>(ticket->target());
            JSGlobalObject* globalObject = promise->globalObject();
            VM& vm = promise->globalObject()->vm();
            promise->resolve(globalObject, (isOKString ? vm.smallStrings.okString() : vm.smallStrings.timedOutString()));
        });
    }

    void unregister(VM* vm)
    {
        m_registeredVMs.remove(vm);
    }

    size_t waiterListSize(void* ptr)
    {
        return ParkingLot::bucketSize(ptr);
    }

private:
    struct RegisteredVMs {
        WTF_MAKE_FAST_ALLOCATED;

    public:
        void add(VM* vm)
        {
            LockHolder locker(lock);
            set.add(vm);
            if (WaiterListsManagerInternal::verbose)
                dataLogF("WaiterListManager registered vm %p\n", vm);
        }

        void remove(VM* vm)
        {
            LockHolder locker(lock);
            set.remove(vm);
            if (WaiterListsManagerInternal::verbose)
                dataLogF("WaiterListManager unregistered vm %p\n", vm);
        }

        bool contains(VM* vm)
        {
            LockHolder locker(lock);
            return set.contains(vm);
        }

        void dump(PrintStream& out) const
        {
            LockHolder locker(lock);
            out.print("m_registeredVMs (size ", set.size(), "): ");
            for (auto& vm : set)
                out.print(RawPointer(vm), " ");
        }

    private:
        mutable Lock lock;
        HashSet<VM*> set;
    };

    RegisteredVMs m_registeredVMs;
};

} // namespace JSC
