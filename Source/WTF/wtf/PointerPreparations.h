/*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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

#if CPU(ARM64E)
#include <ptrauth.h>
#endif

namespace WTF {

#if COMPILER_HAS_CLANG_BUILTIN(__builtin_get_vtable_pointer)

template<typename T>
ALWAYS_INLINE void* getVTablePointer(T* o) { return __builtin_get_vtable_pointer(o); }

#else // not COMPILER_HAS_CLANG_BUILTIN(__builtin_get_vtable_pointer)

#if CPU(ARM64E)
template<typename T>
ALWAYS_INLINE void* getVTablePointer(T* o) { return __builtin_ptrauth_auth(*(reinterpret_cast<void**>(o)), ptrauth_key_cxx_vtable_pointer, 0); }
#else // not CPU(ARM64E)
template<typename T>
ALWAYS_INLINE void* getVTablePointer(T* o) { return (*(reinterpret_cast<void**>(o))); }
#endif // not CPU(ARM64E)

#endif // not COMPILER_HAS_CLANG_BUILTIN(__builtin_get_vtable_pointer)

} // namespace WTF

using WTF::getVTablePointer;
