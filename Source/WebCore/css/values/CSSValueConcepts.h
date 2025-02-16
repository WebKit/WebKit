/*
 * Copyright (C) 2024-2025 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <concepts>

namespace WebCore {

// Types can specialize this and set the value to true to be treated as "empty-like"
// for CSS value type algorithms.
// Requirements: None.
template<typename> inline constexpr auto TreatAsEmptyLike = false;

// The `EmptyLike` concept can be used to filter to types that specialize `TreatAsEmptyLike`.
template<typename T> concept EmptyLike = TreatAsEmptyLike<T>;

// Types can specialize this and set the value to true to be treated as "optional-like"
// for CSS value type algorithms.
// Requirements: Types be comparable to bool and have a operator* function.
template<typename> inline constexpr auto TreatAsOptionalLike = false;

// The `OptionalLike` concept can be used to filter to types that specialize `TreatAsOptionalLike`.
template<typename T> concept OptionalLike = TreatAsOptionalLike<T>;

// Types can specialize this and set the value to true to be treated as "tuple-like"
// for CSS value type algorithms.
// NOTE: This gets automatically specialized when using the *_TUPLE_LIKE_CONFORMANCE macros.
// Requirements: Types must have conform the to the standard tuple-like pseudo-protocol.
template<typename> inline constexpr auto TreatAsTupleLike = false;

// The `TupleLike` concept can be used to filter to types that specialize `TreatAsTupleLike`.
template<typename T> concept TupleLike = TreatAsTupleLike<T>;

// Types can specialize this and set the value to true to be treated as "range-like"
// for CSS value type algorithms.
// Requirements: Types must have valid begin()/end() functions.
template<typename> inline constexpr auto TreatAsRangeLike = false;

// The `RangeLike` concept can be used to filter to types that specialize `TreatAsRangeLike`.
template<typename T> concept RangeLike = TreatAsRangeLike<T>;

// Types can specialize this and set the value to true to be treated as "variant-like"
// for CSS value type algorithms.
// Requirements: Types must be able to be passed to WTF::switchOn().
template<typename> inline constexpr auto TreatAsVariantLike = false;

// The `VariantLike` concept can be used to filter to types that specialize `TreatAsVariantLike`.
template<typename T> concept VariantLike = TreatAsVariantLike<T>;

// The `HasIsZero` concept can be used to filter to types that have an `isZero` member function.
template<typename T> concept HasIsZero = requires(T t) {
    { t.isZero() } -> std::convertible_to<bool>;
};

// The `HasIsEmpty` concept can be used to filter to types that have an `isEmpty` member function.
template<typename T> concept HasIsEmpty = requires(T t) {
    { t.isEmpty() } -> std::convertible_to<bool>;
};

} // namespace WebCore
