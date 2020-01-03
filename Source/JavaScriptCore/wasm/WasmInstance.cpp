/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "WasmInstance.h"

#if ENABLE(WEBASSEMBLY)

#include "JSCInlines.h"
#include "JSWebAssemblyHelpers.h"
#include "JSWebAssemblyInstance.h"
#include "Register.h"
#include "WasmModuleInformation.h"
#include <wtf/CheckedArithmetic.h>

namespace JSC { namespace Wasm {

namespace {
size_t globalMemoryByteSize(Module& module)
{
    return (Checked<size_t>(module.moduleInformation().globals.size()) * sizeof(Register)).unsafeGet();
}
}

Instance::Instance(Context* context, Ref<Module>&& module, EntryFrame** pointerToTopEntryFrame, void** pointerToActualStackLimit, StoreTopCallFrameCallback&& storeTopCallFrame)
    : m_context(context)
    , m_module(WTFMove(module))
    , m_globals(MallocPtr<Global::Value, VMMalloc>::malloc(globalMemoryByteSize(m_module.get())))
    , m_globalsToMark(m_module.get().moduleInformation().globals.size())
    , m_globalsToBinding(m_module.get().moduleInformation().globals.size())
    , m_pointerToTopEntryFrame(pointerToTopEntryFrame)
    , m_pointerToActualStackLimit(pointerToActualStackLimit)
    , m_storeTopCallFrame(WTFMove(storeTopCallFrame))
    , m_numImportFunctions(m_module->moduleInformation().importFunctionCount())
{
    for (unsigned i = 0; i < m_numImportFunctions; ++i)
        new (importFunctionInfo(i)) ImportFunctionInfo();
    memset(static_cast<void*>(m_globals.get()), 0, globalMemoryByteSize(m_module.get()));
    for (unsigned i = 0; i < m_module->moduleInformation().globals.size(); ++i) {
        const Wasm::GlobalInformation& global = m_module.get().moduleInformation().globals[i];
        if (global.bindingMode == Wasm::GlobalInformation::BindingMode::Portable) {
            // This is kept alive by JSWebAssemblyInstance -> JSWebAssemblyGlobal -> binding.
            m_globalsToBinding.set(i);
        } else if (isSubtype(global.type, Anyref)) {
            // This is kept alive by JSWebAssemblyInstance -> binding.
            m_globalsToMark.set(i);
        }
    }
    memset(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, 0), 0, m_module->moduleInformation().tableCount() * sizeof(Table*));
}

Ref<Instance> Instance::create(Context* context, Ref<Module>&& module, EntryFrame** pointerToTopEntryFrame, void** pointerToActualStackLimit, StoreTopCallFrameCallback&& storeTopCallFrame)
{
    return adoptRef(*new (NotNull, fastMalloc(allocationSize(module->moduleInformation().importFunctionCount(), module->moduleInformation().tableCount()))) Instance(context, WTFMove(module), pointerToTopEntryFrame, pointerToActualStackLimit, WTFMove(storeTopCallFrame)));
}

Instance::~Instance() { }

size_t Instance::extraMemoryAllocated() const
{
    return globalMemoryByteSize(m_module.get()) + allocationSize(m_numImportFunctions, m_module->moduleInformation().tableCount());
}

void Instance::setGlobal(unsigned i, JSValue value)
{
    Global::Value* slot = m_globals.get() + i;
    if (m_globalsToBinding.get(i)) {
        Wasm::Global* global = getGlobalBinding(i);
        if (!global)
            return;
        global->valuePointer()->m_anyref.set(owner<JSWebAssemblyInstance>()->vm(), global->owner<JSWebAssemblyGlobal>(), value);
        return;
    }
    ASSERT(m_owner);
    slot->m_anyref.set(owner<JSWebAssemblyInstance>()->vm(), owner<JSWebAssemblyInstance>(), value);
}

JSValue Instance::getFunctionWrapper(unsigned i) const
{
    JSValue value = m_functionWrappers.get(i).get();
    if (value.isEmpty())
        return jsNull();
    return value;
}

void Instance::setFunctionWrapper(unsigned i, JSValue value)
{
    ASSERT(m_owner);
    ASSERT(value.isFunction(owner<JSWebAssemblyInstance>()->vm()));
    ASSERT(!m_functionWrappers.contains(i));
    auto locker = holdLock(owner<JSWebAssemblyInstance>()->cellLock());
    m_functionWrappers.set(i, WriteBarrier<Unknown>(owner<JSWebAssemblyInstance>()->vm(), owner<JSWebAssemblyInstance>(), value));
    ASSERT(getFunctionWrapper(i) == value);
}

Table* Instance::table(unsigned i)
{
    RELEASE_ASSERT(i < m_module->moduleInformation().tableCount());
    return *bitwise_cast<Table**>(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, i));
}

void Instance::setTable(unsigned i, Ref<Table>&& table)
{
    RELEASE_ASSERT(i < m_module->moduleInformation().tableCount());
    ASSERT(!this->table(i));
    *bitwise_cast<Table**>(bitwise_cast<char*>(this) + offsetOfTablePtr(m_numImportFunctions, i)) = &table.leakRef();
}

void Instance::linkGlobal(unsigned i, Ref<Global>&& global)
{
    m_globals.get()[i].m_pointer = global->valuePointer();
    m_linkedGlobals.set(i, WTFMove(global));
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
