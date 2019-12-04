/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "WriteBarrier.h"
#include <wtf/Ref.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC { namespace Wasm {

class Instance;

class Global final : public ThreadSafeRefCounted<Global> {
    WTF_MAKE_NONCOPYABLE(Global);
    WTF_MAKE_FAST_ALLOCATED(Global);
public:
    union Value {
        uint64_t m_primitive;
        WriteBarrierBase<Unknown> m_anyref;
        Value* m_pointer;
    };

    static Ref<Global> create(Wasm::Type type, Wasm::GlobalInformation::Mutability mutability, uint64_t initialValue = 0)
    {
        return adoptRef(*new Global(type, mutability, initialValue));
    }

    Wasm::Type type() const { return m_type; }
    Wasm::GlobalInformation::Mutability mutability() const { return m_mutability; }
    JSValue get() const;
    uint64_t getPrimitive() const { return m_value.m_primitive; }
    void set(JSGlobalObject*, JSValue);
    void visitAggregate(SlotVisitor&);

    template<typename T> T* owner() const { return reinterpret_cast<T*>(m_owner); }
    void setOwner(JSObject* owner)
    {
        ASSERT(!m_owner);
        ASSERT(owner);
        m_owner = owner;
    }

    static ptrdiff_t offsetOfValue() { return OBJECT_OFFSETOF(Global, m_value); }
    static ptrdiff_t offsetOfOwner() { return OBJECT_OFFSETOF(Global, m_owner); }

    static Global& fromBinding(Value& value)
    {
        return *bitwise_cast<Global*>(bitwise_cast<uint8_t*>(&value) - offsetOfValue());
    }

    Value* valuePointer() { return &m_value; }

private:
    Global(Wasm::Type type, Wasm::GlobalInformation::Mutability mutability, uint64_t initialValue)
        : m_type(type)
        , m_mutability(mutability)
        , m_value()
    {
        m_value.m_primitive = initialValue;
    }

    Wasm::Type m_type;
    Wasm::GlobalInformation::Mutability m_mutability;
    JSObject* m_owner { nullptr };
    Value m_value;
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
