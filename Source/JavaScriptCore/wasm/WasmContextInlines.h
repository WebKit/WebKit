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

#include "Options.h"
#include "WasmContext.h"
#include "WasmInstance.h"
#include <mutex>
#include <wtf/FastTLS.h>

namespace JSC { namespace Wasm {

inline bool Context::useFastTLS()
{
#if ENABLE(FAST_TLS_JIT)
    return Options::useFastTLSForWasmContext();
#else
    return false;
#endif
}

inline Instance* Context::load() const
{
#if ENABLE(FAST_TLS_JIT)
    if (useFastTLS())
        return bitwise_cast<Instance*>(_pthread_getspecific_direct(WTF_WASM_CONTEXT_KEY));
#endif
    return instance;
}

inline void Context::store(Instance* inst)
{
#if ENABLE(FAST_TLS_JIT)
    if (useFastTLS()) {
        _pthread_setspecific_direct(WTF_WASM_CONTEXT_KEY, bitwise_cast<void*>(inst));
        return;
    }
#endif
    instance = inst;
}

inline Instance* Context::tryLoadInstanceFromTLS()
{
#if ENABLE(FAST_TLS_JIT)
    if (useFastTLS())
        return bitwise_cast<Instance*>(_pthread_getspecific_direct(WTF_WASM_CONTEXT_KEY));
#endif
    return nullptr;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
