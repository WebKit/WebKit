/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cstddef>

namespace WTF {

// `IsIncreasing` is a utility to check that a series of numbers are increasing.
//
// On example usage is for generated code to check the relative order of the members of a struct:
//
//   Given struct:
//
//     struct Foo {
//         int a;
//         int b;
//         int c;
//     };
//
//   You can statically assert:
//
//     static_assert(IsIncreasing<
//           0
//         , offsetof(Foo, a)
//         , offsetof(Foo, b)
//         , offsetof(Foo, c)
//     >);

template<size_t...>
struct IsIncreasingChecker;

template<size_t index>
struct IsIncreasingChecker<index> {
    static constexpr bool value = true;
};

template<size_t firstIndex, size_t secondIndex, size_t... remainingIndices>
struct IsIncreasingChecker<firstIndex, secondIndex, remainingIndices...> {
    static constexpr bool value = firstIndex > secondIndex ? false : IsIncreasingChecker<secondIndex, remainingIndices...>::value;
};

template<size_t... indices>
constexpr bool IsIncreasing = IsIncreasingChecker<indices...>::value;

} // namespace WTF

using WTF::IsIncreasing;
