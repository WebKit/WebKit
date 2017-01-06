/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "WasmCallingConvention.h"
#include "WasmPageCount.h"

#include <wtf/Vector.h>

namespace JSC { namespace Wasm {

class Memory {
    WTF_MAKE_NONCOPYABLE(Memory);
    WTF_MAKE_FAST_ALLOCATED;
public:

    // FIXME: We should support other modes. see: https://bugs.webkit.org/show_bug.cgi?id=162693
    enum class Mode {
        BoundsChecking
    };

    JS_EXPORT_PRIVATE Memory(PageCount initial, PageCount maximum);

    ~Memory()
    {
        if (m_memory)
            munmap(m_memory, m_mappedCapacity);
    }

    bool isValid() const { return !!m_memory; }

    void* memory() const { return m_memory; }
    uint32_t size() const { return m_size; }

    Mode mode() const { return m_mode; }

    PageCount initial() const { return m_initial; }
    PageCount maximum() const { return m_maximum; }

    bool grow(uint32_t newSize)
    {
        ASSERT(m_memory);
        if (newSize > m_capacity)
            return false;

        return !mprotect(m_memory, newSize, PROT_READ | PROT_WRITE);
    }

    static ptrdiff_t offsetOfSize() { return OBJECT_OFFSETOF(Memory, m_size); }

    
private:
    void* m_memory { nullptr };
    Mode m_mode;
    uint32_t m_size { 0 };
    uint32_t m_capacity { 0 };
    PageCount m_initial;
    PageCount m_maximum;
    uint64_t m_mappedCapacity { 0 };
};

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMLY)
