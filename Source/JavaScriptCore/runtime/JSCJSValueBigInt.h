/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

// JSValue inline functions that need JSBigInt to be complete.
// This is NOT an Inlines header — it can be freely included by
// any header that needs JSValue BigInt methods.

#include <JavaScriptCore/JSBigInt.h>
#include <JavaScriptCore/JSCJSValue.h>

namespace JSC {

ALWAYS_INLINE JSBigInt* JSValue::asHeapBigInt() const
{
    ASSERT(isHeapBigIntSlow());
#if USE(JSVALUE32_64)
    return reinterpret_cast<JSBigInt*>(u.asBits.payload);
#else // !USE(JSVALUE32_64) i.e. USE(JSVALUE64)
    SUPPRESS_MEMORY_UNSAFE_CAST return static_cast<JSBigInt*>(u.ptr);
#endif // USE(JSVALUE64)
}

} // namespace JSC
