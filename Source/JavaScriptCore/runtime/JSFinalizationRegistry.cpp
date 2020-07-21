/*
 * Copyright (C) 2020 Apple, Inc. All rights reserved.
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
#include "JSFinalizationRegistry.h"

#include "DeferredWorkTimer.h"
#include "JSCInlines.h"
#include "JSInternalFieldObjectImplInlines.h"

namespace JSC {

const ClassInfo JSFinalizationRegistry::s_info = { "FinalizationRegistry", &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSFinalizationRegistry) };

Structure* JSFinalizationRegistry::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSFinalizationRegistry* JSFinalizationRegistry::create(VM& vm, Structure* structure, JSObject* callback)
{
    JSFinalizationRegistry* instance = new (NotNull, allocateCell<JSFinalizationRegistry>(vm.heap)) JSFinalizationRegistry(vm, structure);
    instance->finishCreation(vm, callback);
    return instance;
}

void JSFinalizationRegistry::finishCreation(VM& vm, JSObject* callback)
{
    Base::finishCreation(vm);
    ASSERT(callback->isCallable(vm));
    auto values = initialValues();
    for (unsigned index = 0; index < values.size(); ++index)
        Base::internalField(index).setWithoutWriteBarrier(values[index]);
    internalField(Field::Callback).setWithoutWriteBarrier(callback);
}

void JSFinalizationRegistry::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    Base::visitChildren(cell, visitor);

    auto* thisObject = jsCast<JSFinalizationRegistry*>(cell);

    auto locker = holdLock(thisObject->cellLock());
    for (const auto& iter : thisObject->m_liveRegistrations) {
        for (auto& registration : iter.value)
            visitor.append(registration.holdings);
    }
    for (auto& registration : thisObject->m_noUnregistrationLive)
        visitor.append(registration.holdings);
    for (const auto& iter : thisObject->m_deadRegistrations) {
        for (auto& holdings : iter.value)
            visitor.append(holdings);
    }
    for (auto& holdings : thisObject->m_noUnregistrationDead)
        visitor.append(holdings);

    size_t totalBufferSizesInBytes = thisObject->m_deadRegistrations.capacity() * sizeof(decltype(thisObject->m_deadRegistrations)::KeyValuePairType);
    totalBufferSizesInBytes += thisObject->m_liveRegistrations.capacity() * sizeof(decltype(thisObject->m_deadRegistrations)::KeyValuePairType);
    totalBufferSizesInBytes += thisObject->m_noUnregistrationLive.capacity() * sizeof(decltype(thisObject->m_noUnregistrationLive.takeLast()));
    totalBufferSizesInBytes += thisObject->m_noUnregistrationDead.capacity() * sizeof(decltype(thisObject->m_noUnregistrationLive.takeLast()));
    visitor.vm().heap.reportExtraMemoryVisited(totalBufferSizesInBytes);
}

void JSFinalizationRegistry::destroy(JSCell* table)
{
    static_cast<JSFinalizationRegistry*>(table)->~JSFinalizationRegistry();
}

void JSFinalizationRegistry::finalizeUnconditionally(VM& vm)
{
    auto locker = holdLock(cellLock());

#if ASSERT_ENABLED
    for (const auto& iter : m_deadRegistrations)
        RELEASE_ASSERT(iter.value.size());
#endif

    bool readiedCell = false;
    m_noUnregistrationLive.removeAllMatching([&] (const Registration& reg) {
        ASSERT(!reg.holdings.get().isCell() || vm.heap.isMarked(reg.holdings.get().asCell()));
        if (!vm.heap.isMarked(reg.target)) {
            m_noUnregistrationDead.append(reg.holdings);
            readiedCell = true;
            return true;
        }
        return false;
    });

    m_liveRegistrations.removeIf([&] (auto& bucket) -> bool {
        ASSERT(bucket.value.size());

        bool keyIsDead = !vm.heap.isMarked(bucket.key);
        DeadRegistrations* deadList = nullptr;
        auto getDeadList = [&] () -> DeadRegistrations& {
            if (UNLIKELY(!deadList))
                deadList = &m_deadRegistrations.add(bucket.key, DeadRegistrations()).iterator->value;
            return *deadList;
        };

        bucket.value.removeAllMatching([&] (const Registration& reg) {
            ASSERT(!reg.holdings.get().isCell() || vm.heap.isMarked(reg.holdings.get().asCell()));
            if (!vm.heap.isMarked(reg.target)) {
                if (keyIsDead)
                    m_noUnregistrationDead.append(reg.holdings);
                else
                    getDeadList().append(reg.holdings);
                readiedCell = true;
                return true;
            }

            if (keyIsDead) {
                m_noUnregistrationLive.append(reg);
                return true;
            }

            return false;
        });

        return !bucket.value.size();
    });

    if (!vm.deferredWorkTimer->hasPendingWork(this) && (readiedCell || deadCount(locker))) {
        vm.deferredWorkTimer->addPendingWork(vm, this, { });
        ASSERT(vm.deferredWorkTimer->hasPendingWork(this));
        vm.deferredWorkTimer->scheduleWorkSoon(this, [this] {
            JSGlobalObject* globalObject = this->globalObject();
            VM& vm = globalObject->vm();
            this->runFinalizationCleanup(globalObject);
            vm.deferredWorkTimer->cancelPendingWork(this);
        });
    }
}

void JSFinalizationRegistry::runFinalizationCleanup(JSGlobalObject* globalObject)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    while (JSValue value = takeDeadHoldingsValue()) {
        MarkedArgumentBuffer args;
        args.append(value);
        call(globalObject, callback(), args, "This should not be visible: please report a bug to bugs.webkit.org");
        RETURN_IF_EXCEPTION(scope, void());
    }
}

JSValue JSFinalizationRegistry::takeDeadHoldingsValue()
{
    auto locker = holdLock(cellLock());
    JSValue result;
    if (m_noUnregistrationDead.size())
        result = m_noUnregistrationDead.takeLast().get();
    else {
        auto iter = m_deadRegistrations.begin();
        if (iter == m_deadRegistrations.end())
            return JSValue();
        ASSERT(iter->value.size());
        result = iter->value.takeLast().get();
        if (!iter->value.size())
            m_deadRegistrations.remove(iter);
    }

#if ASSERT_ENABLED
    for (const auto& iter : m_deadRegistrations)
        RELEASE_ASSERT(iter.value.size());
#endif

    return result;
}

void JSFinalizationRegistry::registerTarget(VM& vm, JSObject* target, JSValue holdings, JSValue token)
{
    auto locker = holdLock(cellLock());
    Registration registration;
    registration.target = target;
    registration.holdings.setWithoutWriteBarrier(holdings);
    if (token.isUndefined())
        m_noUnregistrationLive.append(WTFMove(registration));
    else {
        auto result = m_liveRegistrations.add(jsSecureCast<JSObject*>(vm, token), LiveRegistrations());
        result.iterator->value.append(WTFMove(registration));
    }
    vm.heap.writeBarrier(this);
}

bool JSFinalizationRegistry::unregister(VM&, JSObject* token)
{
    // We don't need to write barrier ourselves here because we will only point to less things after this finishes.
    auto locker = holdLock(cellLock());
    bool result = m_liveRegistrations.remove(token);
    result |= m_deadRegistrations.remove(token);

    return result;
}

size_t JSFinalizationRegistry::liveCount(const Locker<JSCellLock>&)
{
    size_t count = m_noUnregistrationLive.size();
    for (const auto& iter : m_liveRegistrations)
        count += iter.value.size();

    return count;
}

size_t JSFinalizationRegistry::deadCount(const Locker<JSCellLock>&)
{
    size_t count = m_noUnregistrationDead.size();
    for (const auto& iter : m_deadRegistrations)
        count += iter.value.size();

    return count;
}

String JSFinalizationRegistry::toStringName(const JSC::JSObject*, JSGlobalObject*)
{
    return "Object"_s;
}

}


