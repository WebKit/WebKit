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
#include "ModuleLoaderPayload.h"

#include "JSCellInlines.h"
#include "JSPromise.h"

namespace JSC {

const ClassInfo ModuleLoaderPayload::s_info = { "ModuleLoaderPayload"_s, nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(ModuleLoaderPayload) };

ModuleLoaderPayload::ModuleLoaderPayload(VM& vm, Structure* structure, ModuleGraphLoadingState* state)
    : Base(vm, structure)
    , m_payload(WriteBarrier(state, WriteBarrierEarlyInit))
{
}

ModuleLoaderPayload::ModuleLoaderPayload(VM& vm, Structure* structure, JSPromise* promise)
    : Base(vm, structure)
    , m_payload(WriteBarrier(promise, WriteBarrierEarlyInit))
{
}

void ModuleLoaderPayload::destroy(JSCell* cell)
{
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<ModuleLoaderPayload*>(cell);
    thisObject->~ModuleLoaderPayload();
}

void ModuleLoaderPayload::finishCreation(VM& vm)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
}

template<typename Visitor>
void ModuleLoaderPayload::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<ModuleLoaderPayload>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    visitor.append(thisObject->m_fulfillment);
    if (auto* state = std::get_if<WriteBarrier<ModuleGraphLoadingState>>(&thisObject->m_payload))
        visitor.append(*state);
    else {
        ASSERT(std::holds_alternative<WriteBarrier<JSPromise>>(thisObject->m_payload));
        visitor.append(std::get<WriteBarrier<JSPromise>>(thisObject->m_payload));
    }
}

DEFINE_VISIT_CHILDREN(ModuleLoaderPayload);

ModuleLoaderPayload* ModuleLoaderPayload::create(VM& vm, ModuleGraphLoadingState* state)
{
    ModuleLoaderPayload* instance = new (NotNull, allocateCell<ModuleLoaderPayload>(vm)) ModuleLoaderPayload(vm, vm.moduleLoaderPayloadStructure.get(), state);
    instance->finishCreation(vm);
    return instance;
}

ModuleLoaderPayload* ModuleLoaderPayload::create(VM& vm, JSPromise* promise)
{
    ModuleLoaderPayload* instance = new (NotNull, allocateCell<ModuleLoaderPayload>(vm)) ModuleLoaderPayload(vm, vm.moduleLoaderPayloadStructure.get(), promise);
    instance->finishCreation(vm);
    return instance;
}

ModuleGraphLoadingState* ModuleLoaderPayload::getState() const
{
    auto option = std::get_if<WriteBarrier<ModuleGraphLoadingState>>(&m_payload);
    return option ? option->get() : nullptr;
}

JSPromise* ModuleLoaderPayload::getPromise() const
{
    auto option = std::get_if<WriteBarrier<JSPromise>>(&m_payload);
    return option ? option->get() : nullptr;
}

bool ModuleLoaderPayload::isState() const
{
    return std::holds_alternative<WriteBarrier<ModuleGraphLoadingState>>(m_payload);
}

bool ModuleLoaderPayload::isPromise() const
{
    return std::holds_alternative<WriteBarrier<JSPromise>>(m_payload);
}

JSPromise* ModuleLoaderPayload::underlyingPromise() const
{
    return isState() ? getState()->promise() : getPromise();
}

JSValue ModuleLoaderPayload::fulfillment() const
{
    return m_fulfillment.get();
}

void ModuleLoaderPayload::fulfillment(VM& vm, JSValue value)
{
    m_fulfillment.set(vm, this, value);
}

bool ModuleLoaderPayload::decrementRemaining()
{
    ASSERT(m_remainingFulfillments > 0);
    return !--m_remainingFulfillments;
}

} // namespace JSC
