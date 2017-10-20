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

#pragma once

#if ENABLE(WEBASSEMBLY)

#include "WasmFormat.h"
#include "WasmMemory.h"
#include "WasmModule.h"
#include "WasmTable.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Instance : public ThreadSafeRefCounted<Instance> {
public:
    static Ref<Instance> create(Ref<Module>&& module)
    {
        return adoptRef(*new Instance(WTFMove(module)));
    }

    void finalizeCreation(Ref<CodeBlock>&& codeBlock)
    {
        m_codeBlock = WTFMove(codeBlock);
    }

    JS_EXPORT_PRIVATE ~Instance();

    size_t extraMemoryAllocated() const;

    Module& module() { return m_module.get(); }
    CodeBlock* codeBlock() { return m_codeBlock.get(); }
    Memory* memory() { return m_memory.get(); }
    Table* table() { return m_table.get(); }

    int32_t loadI32Global(unsigned i) const { return m_globals.get()[i]; }
    int64_t loadI64Global(unsigned i) const { return m_globals.get()[i]; }
    float loadF32Global(unsigned i) const { return bitwise_cast<float>(loadI32Global(i)); }
    double loadF64Global(unsigned i) const { return bitwise_cast<double>(loadI64Global(i)); }
    void setGlobal(unsigned i, int64_t bits) { m_globals.get()[i] = bits; }

    static ptrdiff_t offsetOfCachedStackLimit() { return OBJECT_OFFSETOF(Instance, m_cachedStackLimit); }
    void* cachedStackLimit() const { return m_cachedStackLimit; }
    void setCachedStackLimit(void* limit) { m_cachedStackLimit = limit; }

    friend class JSC::JSWebAssemblyInstance; // FIXME remove this once refactored https://webkit.org/b/177472.

private:
    Instance(Ref<Module>&&);

    Ref<Module> m_module;
    RefPtr<CodeBlock> m_codeBlock;
    RefPtr<Memory> m_memory;
    RefPtr<Table> m_table;
    MallocPtr<uint64_t> m_globals;
    void* m_cachedStackLimit { bitwise_cast<void*>(std::numeric_limits<uintptr_t>::max()) };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
