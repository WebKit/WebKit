// Copyright 2025 The ANGLE Project Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_SPAN_UTIL_H_
#define COMMON_SPAN_UTIL_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "common/base/anglebase/logging.h"
#include "common/span.h"
#include "common/unsafe_buffers.h"

namespace angle
{

// Bounds-checked byte-for-byte copies from spans into spans.
template <typename T1, typename T2, size_t N1, size_t N2>
inline void SpanMemcpy(angle::Span<T1, N1> dst, angle::Span<T2, N2> src)
{
    static_assert(sizeof(T1) == sizeof(T2) && std::is_trivially_copyable_v<T1> &&
                  std::is_trivially_copyable_v<T2>);
    CHECK(dst.size() >= src.size());
    if (src.size())
    {
        // SAFETY: static_assert() ensures `sizeof(T1)` equals `sizeof(T2)`, so
        // comparing `size()` for equality ensures `size_bytes()` are equal, and
        // `size_bytes()` accurately describes `data()`.
        ANGLE_UNSAFE_BUFFERS(memcpy(dst.data(), src.data(), src.size_bytes()));
    }
}

// Bounds-checked byte-for-byte moves from spans into spans.
template <typename T1, typename T2, size_t N1, size_t N2>
inline void SpanMemmove(angle::Span<T1, N1> dst, angle::Span<T2, N2> src)
{
    static_assert(sizeof(T1) == sizeof(T2) && std::is_trivially_copyable_v<T1> &&
                  std::is_trivially_copyable_v<T2>);
    CHECK(dst.size() >= src.size());
    if (src.size())
    {
        // SAFETY: static_assert()  ensures `sizeof(T1)` equals `sizeof(T2)`, so
        // comparing `size()` for equality ensures `size_bytes()` are equal, and
        // `size_bytes()` accurately describes `data()`.
        ANGLE_UNSAFE_BUFFERS(memmove(dst.data(), src.data(), src.size_bytes()));
    }
}

// Bounds-checked memsets into spans.
template <typename T, size_t N>
inline void SpanMemset(angle::Span<T, N> dst, uint8_t val)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (dst.size())
    {
        // SAFETY: `dst.data()` is valid for `dst.size_bytes()` bytes.
        ANGLE_UNSAFE_BUFFERS(memset(dst.data(), val, dst.size_bytes()));
    }
}

}  // namespace angle

#endif  // COMMON_SPAN_UTIL_H_
