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
#include <wtf/StdLibExtras.h>

namespace JSC {

namespace Wasm {
class Callee;
}

class JSCell;

class CalleeBits {
public:
    CalleeBits() = default;
    CalleeBits(void* ptr) : m_ptr(ptr) { } 

    CalleeBits& operator=(JSCell* cell)
    {
        m_ptr = cell;
        ASSERT(isCell());
        return *this;
    }

#if ENABLE(WEBASSEMBLY)
    static void* boxWasm(Wasm::Callee* callee)
    {
        CalleeBits result(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(callee) | TagBitsWasm));
        ASSERT(result.isWasm());
        return result.rawPtr();
    }
#endif

    bool isWasm() const
    {
#if ENABLE(WEBASSEMBLY)
        return (reinterpret_cast<uintptr_t>(m_ptr) & TagWasmMask) == TagBitsWasm;
#else
        return false;
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
        return reinterpret_cast<Wasm::Callee*>(reinterpret_cast<uintptr_t>(m_ptr) & ~TagBitsWasm);
    }
#endif

    void* rawPtr() const { return m_ptr; }
    
private:
    void* m_ptr { nullptr };
};

} // namespace JSC
