/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
    , m_globals(MallocPtr<uint64_t>::malloc(globalMemoryByteSize(m_module.get())))
    , m_pointerToTopEntryFrame(pointerToTopEntryFrame)
    , m_pointerToActualStackLimit(pointerToActualStackLimit)
    , m_storeTopCallFrame(WTFMove(storeTopCallFrame))
    , m_numImportFunctions(m_module->moduleInformation().importFunctionCount())
{
    for (unsigned i = 0; i < m_numImportFunctions; ++i)
        new (importFunctionInfo(i)) ImportFunctionInfo();
}

Ref<Instance> Instance::create(Context* context, Ref<Module>&& module, EntryFrame** pointerToTopEntryFrame, void** pointerToActualStackLimit, StoreTopCallFrameCallback&& storeTopCallFrame)
{
    return adoptRef(*new (NotNull, fastMalloc(allocationSize(module->moduleInformation().importFunctionCount()))) Instance(context, WTFMove(module), pointerToTopEntryFrame, pointerToActualStackLimit, WTFMove(storeTopCallFrame)));
}

Instance::~Instance() { }

size_t Instance::extraMemoryAllocated() const
{
    return globalMemoryByteSize(m_module.get()) + allocationSize(m_numImportFunctions);
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
