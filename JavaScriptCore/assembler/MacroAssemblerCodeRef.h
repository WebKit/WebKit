/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef MacroAssemblerCodeRef_h
#define MacroAssemblerCodeRef_h

#include <wtf/Platform.h>

#include "ExecutableAllocator.h"
#include "PassRefPtr.h"
#include "RefPtr.h"
#include "UnusedParam.h"

#if ENABLE(ASSEMBLER)

namespace JSC {

// MacroAssemblerCodeRef:
//
// A reference to a section of JIT generated code.  A CodeRef consists of a
// pointer to the code, and a ref pointer to the pool from within which it
// was allocated.
class MacroAssemblerCodeRef {
public:
    MacroAssemblerCodeRef()
        : m_code(0)
#ifndef NDEBUG
        , m_size(0)
#endif
    {
    }

    MacroAssemblerCodeRef(void* code, PassRefPtr<ExecutablePool> executablePool, size_t size)
        : m_code(code)
        , m_executablePool(executablePool)
    {
#ifndef NDEBUG
        m_size = size;
#else
        UNUSED_PARAM(size);
#endif
    }

    void* m_code;
    RefPtr<ExecutablePool> m_executablePool;
#ifndef NDEBUG
    size_t m_size;
#endif
};

} // namespace JSC

#endif // ENABLE(ASSEMBLER)

#endif // MacroAssemblerCodeRef_h
