/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#ifndef StdLibExtras_h
#define StdLibExtras_h

#include "BCompiler.h"
#include <memory>
#include <type_traits>

namespace bmalloc {

template<typename ToType, typename FromType>
inline ToType bitwise_cast(FromType from)
{
    static_assert(sizeof(FromType) == sizeof(ToType), "bitwise_cast size of FromType and ToType must be equal!");
#if BCOMPILER_HAS_CLANG_FEATURE(is_trivially_copyable)
    // Not all recent STL implementations support the std::is_trivially_copyable type trait. Work around this by only checking on toolchains which have the equivalent compiler intrinsic.
    static_assert(__is_trivially_copyable(ToType), "bitwise_cast of non-trivially-copyable type!");
    static_assert(__is_trivially_copyable(FromType), "bitwise_cast of non-trivially-copyable type!");
#endif
    typename std::remove_const<ToType>::type to { };
    std::memcpy(static_cast<void*>(&to), static_cast<void*>(&from), sizeof(to));
    return to;
}

} // namespace bmalloc

#endif // StdLibExtras_h
