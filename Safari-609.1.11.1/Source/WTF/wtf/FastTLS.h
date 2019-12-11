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

#if HAVE(FAST_TLS)

#include <pthread.h>
#include <System/pthread_machdep.h>
#include <wtf/Platform.h>

namespace WTF {

// __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY0-1 is taken by bmalloc, so WTF's KEY0 maps to the
// system's KEY2.
#define WTF_FAST_TLS_KEY0 __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY2
#define WTF_FAST_TLS_KEY1 __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY3
#define WTF_FAST_TLS_KEY2 __PTK_FRAMEWORK_JAVASCRIPTCORE_KEY4

// NOTE: We should manage our use of these keys here. If you want to use a key for something,
// put a #define in here to give your key a symbolic name. This ensures that we don't
// accidentally use the same key for more than one thing.

#define WTF_THREAD_DATA_KEY WTF_FAST_TLS_KEY0
#define WTF_WASM_CONTEXT_KEY WTF_FAST_TLS_KEY1
#define WTF_TESTING_KEY WTF_WASM_CONTEXT_KEY // So far, this key is only used in places that don't do WebAssembly, so it's OK that they share the same key.
#define WTF_GC_TLC_KEY WTF_FAST_TLS_KEY2

#if ENABLE(FAST_TLS_JIT)
inline unsigned fastTLSOffsetForKey(unsigned long slot)
{
    return slot * sizeof(void*);
}
#endif

} // namespace WTF

#if ENABLE(FAST_TLS_JIT)
using WTF::fastTLSOffsetForKey;
#endif

#endif // HAVE(FAST_TLS)

