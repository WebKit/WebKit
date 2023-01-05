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

#include "JSCJSValue.h"
#include <wtf/AccessibleAddress.h>
#include <wtf/StdLibExtras.h>

namespace JSC {

namespace Wasm {
class Callee;
}

class JSCell;

class CalleeBits {
public:
    CalleeBits() = default;
    CalleeBits(int64_t value)
#if USE(JSVALUE64)
        : m_ptr { reinterpret_cast<void*>(value) }
#elif USE(JSVALUE32_64)
        : m_ptr { reinterpret_cast<void*>(JSValue::decode(value).payload()) }
        , m_tag { JSValue::decode(value).tag() }
#endif
    { }

    CalleeBits& operator=(JSCell* cell)
    {
        m_ptr = cell;
#if USE(JSVALUE32_64)
        m_tag = JSValue::CellTag;
#endif
        ASSERT(isCell());
        return *this;
    }

#if ENABLE(WEBASSEMBLY)
    static void* boxWasm(Wasm::Callee* callee)
    {
#if USE(JSVALUE64)
        CalleeBits result { static_cast<int64_t>((bitwise_cast<uintptr_t>(callee) - lowestAccessibleAddress()) | JSValue::WasmTag) };
        ASSERT(result.isWasm());
        return result.rawPtr();
#elif USE(JSVALUE32_64)
        return bitwise_cast<void*>(bitwise_cast<uintptr_t>(callee) - lowestAccessibleAddress());
#endif
    }
#endif

    bool isWasm() const
    {
#if !ENABLE(WEBASSEMBLY)
        return false;
#elif USE(JSVALUE64)
        return (reinterpret_cast<uintptr_t>(m_ptr) & JSValue::WasmMask) == JSValue::WasmTag;
#elif USE(JSVALUE32_64)
        return m_tag == JSValue::WasmTag;
#endif
    }
    bool isCell() const { return !isWasm(); }

    JSCell* asCell() const
    {
        ASSERT(!isWasm());
        return static_cast<JSCell*>(m_ptr);
    }

#if ENABLE(WEBASSEMBLY)
    Wasm::Callee* asWasmCallee() const
    {
        ASSERT(isWasm());
#if USE(JSVALUE64)
        return bitwise_cast<Wasm::Callee*>((bitwise_cast<uintptr_t>(m_ptr) & ~JSValue::WasmTag) + lowestAccessibleAddress());
#elif USE(JSVALUE32_64)
        return bitwise_cast<Wasm::Callee*>(bitwise_cast<uintptr_t>(m_ptr) + lowestAccessibleAddress());
#endif
    }
#endif

    void* rawPtr() const { return m_ptr; }

private:
    void* m_ptr { nullptr };
#if USE(JSVALUE32_64)
    uint32_t m_tag { JSValue::EmptyValueTag };
#endif
};

} // namespace JSC
