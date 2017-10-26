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
#include "WasmLimits.h"
#include <wtf/MallocPtr.h>
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Instance;

class Table : public ThreadSafeRefCounted<Table> {
public:
    static RefPtr<Table> create(uint32_t initial, std::optional<uint32_t> maximum);

    JS_EXPORT_PRIVATE ~Table();

    std::optional<uint32_t> maximum() const { return m_maximum; }
    uint32_t size() const { return m_size; }
    std::optional<uint32_t> grow(uint32_t delta) WARN_UNUSED_RETURN;
    void clearFunction(uint32_t);
    void setFunction(uint32_t, CallableFunction, Instance*);

    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(Table, m_size); }
    static ptrdiff_t offsetOfFunctions() { return OBJECT_OFFSETOF(Table, m_functions); }
    static ptrdiff_t offsetOfInstances() { return OBJECT_OFFSETOF(Table, m_instances); }

    static bool isValidSize(uint32_t size) { return size < maxTableEntries; }

private:
    Table(uint32_t initial, std::optional<uint32_t> maximum);

    std::optional<uint32_t> m_maximum;
    uint32_t m_size;
    MallocPtr<CallableFunction> m_functions;
    // call_indirect needs to do an Instance check to potentially context switch when calling a function to another instance. We can hold raw pointers to Instance here because the embedder ensures that Table keeps all the instances alive. We couldn't hold a Ref here because it would cause cycles.
    MallocPtr<Instance*> m_instances;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
